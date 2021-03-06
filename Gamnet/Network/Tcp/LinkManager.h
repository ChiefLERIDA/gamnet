#ifndef GAMNET_NETWORK_TCP_LINKMANAGER_H_
#define GAMNET_NETWORK_TCP_LINKMANAGER_H_

#include "Dispatcher.h"
#include "Packet.h"
#include "Link.h"
#include "../LinkManager.h"
#include "../../Library/Json/json.h"

#include <memory>
#include <atomic>
#include <mutex>

namespace Gamnet { namespace Network { namespace Tcp {

template <class SESSION_T>
class LinkManager : public Network::LinkManager
{
public :
	enum MSG_ID {
		MsgID_CliSvr_Connect_Req = 1,
		MsgID_SvrCli_Connect_Ans = 1,
		MsgID_CliSvr_Reconnect_Req = 2,
		MsgID_SvrCli_Reconnect_Ans = 2,
		MsgID_CliSvr_HeartBeat_Req = 3,
		MsgID_SvrCli_HeartBeat_Ans = 3,
		MsgID_SvrCli_Kickout_Ntf = 4,
		MsgID_CliSvr_Close_Ntf = 5,
		MsgID_Max = 6
	};
private :
	struct LinkFactory {
		LinkManager* const link_manager;
		LinkFactory(LinkManager* linkManager) : link_manager(linkManager) {}

		Link* operator() ()
		{
			return new Link(link_manager);
		}
	};

	std::function<void(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer)> handlers[MSG_ID::MsgID_Max];

public :
	SessionManager session_manager;
	Pool<SESSION_T, std::mutex, Network::Session::InitFunctor> session_pool;
	Pool<Link, std::mutex, Network::Link::InitFunctor> link_pool;

	LinkManager() : session_pool(), link_pool(65535, LinkFactory(this))
	{
		name = "Gamnet::Network::Tcp::LinkManager";
	}

	virtual ~LinkManager()	
	{
	}

	void Listen(int port, int max_session, int keep_alive_time, int accept_queue_size)
	{
		session_pool.Capacity(max_session);
		session_manager.Init(keep_alive_time);

		link_pool.Capacity(max_session);

		handlers[0] = nullptr;
		handlers[MsgID_CliSvr_Connect_Req] = std::bind(&LinkManager::Recv_Connect_Req, this, std::placeholders::_1, std::placeholders::_2);
		handlers[MsgID_CliSvr_Reconnect_Req] = std::bind(&LinkManager::Recv_Reconnect_Req, this, std::placeholders::_1, std::placeholders::_2);
		handlers[MsgID_CliSvr_HeartBeat_Req] = std::bind(&LinkManager::Recv_HeartBeat_Req, this, std::placeholders::_1, std::placeholders::_2);
		handlers[MsgID_CliSvr_Close_Ntf] = std::bind(&LinkManager::Recv_Close_Ntf, this, std::placeholders::_1, std::placeholders::_2);

		Network::LinkManager::Listen(port, accept_queue_size);
	}

	virtual std::shared_ptr<Network::Link> Create() override
	{
		std::shared_ptr<Link> link = link_pool.Create();
		if(nullptr == link)
		{
			LOG(GAMNET_ERR, "[link_manager:", name, "] can not create 'Tcp::Link' instance");
			return nullptr;
		}
		return link;
	}

	virtual void OnAccept(const std::shared_ptr<Network::Link>& link) override
	{
	}

	virtual void OnClose(const std::shared_ptr<Network::Link>& link, int reason) override
	{
		std::shared_ptr<Network::Session> session = link->session;
		if(nullptr == session)
		{
			return;
		}

		session->strand.wrap([session, reason] () {
			try {
				session->OnClose(reason);
				session->AttachLink(nullptr);
			}
			catch (const Exception& e)
			{
				LOG(Log::Logger::LOG_LEVEL_ERR, e.what(), "(error_code:", e.error_code(), ")");
			}
		})();
	}

	virtual void OnRecvMsg(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer) override
	{
		const std::shared_ptr<Packet>& packet = std::static_pointer_cast<Packet>(buffer);
			
		uint32_t msgSEQ = packet->GetSEQ();
		uint32_t msgID = packet->GetID();

		std::shared_ptr<Link> tcpLink = std::static_pointer_cast<Link>(link);
		if(msgSEQ <= tcpLink->msg_seq)
		{
			LOG(WRN, "[link_key:", link->link_key, "] discard message(msg_id:", packet->GetID(), ", received msg_seq:", msgSEQ, ", expected msg_seq:", tcpLink->msg_seq + 1, ")");
			return;
		}

		tcpLink->msg_seq = msgSEQ;
		
		if(MSG_ID::MsgID_Max <= msgID)
		{
			std::shared_ptr<SESSION_T> session = std::static_pointer_cast<SESSION_T>(link->session);
			if(nullptr == session)
			{
				LOG(ERR, "[link_key:", link->link_key, "] invalid session");
				link->Close(ErrorCode::NullPointerError);
				return;
			}

			session->expire_time = ::time(nullptr);
			session->strand.wrap(std::bind(&Dispatcher<SESSION_T>::OnRecvMsg, &Singleton<Dispatcher<SESSION_T>>::GetInstance(), session, packet))();
		}
		else
		{
			if(nullptr != handlers[msgID])
			{
				handlers[msgID](link, buffer);
			}
		}
	}

	std::shared_ptr<SESSION_T> FindSession(uint32_t session_key) 
	{
		return std::static_pointer_cast<SESSION_T>(session_manager.Find(session_key));
	}
	
	void DestroySession(uint32_t session_key)
	{
		std::shared_ptr<Network::Session> session = session_manager.Find(session_key);
		if (nullptr == session)
		{
			LOG(WRN, "can not find session(session_key:", session_key, ")");
			return;
		}
		
		std::shared_ptr<Network::Link> link = session->link;
		if(nullptr != link)
		{
			link->strand.wrap(std::bind(&Network::Link::Close, link, ErrorCode::Success))();
		}
		
		session->strand.wrap([session](){
			try {
				session->OnDestroy();
			}
			catch (const Exception& e)
			{
				LOG(Log::Logger::LOG_LEVEL_ERR, e.what(), "(error_code:", e.error_code(), ")");
			}
		})(); 
		
		session_manager.Remove(session_key);
	}

	template <class FUNC, class FACTORY>
	bool RegisterHandler(unsigned int msg_id, const char* name, FUNC func, FACTORY factory)
	{
		return Singleton<Dispatcher<SESSION_T>>::GetInstance().RegisterHandler(msg_id, name, func, factory);
	}

	void Recv_Connect_Req(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer) 
	{
		Json::Value ans;
		ans["error_code"] = 0;
		ans["session_key"] = 0;
		ans["session_token"] = "";

		try 
		{
			LOG(DEV, "[link_key:", link->link_key, "]");	
			std::shared_ptr<SESSION_T> session = session_pool.Create();
			if(nullptr == session)
			{
				throw GAMNET_EXCEPTION(ErrorCode::NullPointerError, "[link_key:", link->link_key, "] can not create session instance");
			}

			link->session = session;

			session_manager.Add(session->session_key, session);

			session->strand.wrap([session, link] () {
				try {
					session->OnCreate();
					session->AttachLink(link);
					session->OnAccept();
				}
				catch(const Exception& e)
				{
					LOG(Log::Logger::LOG_LEVEL_ERR, e.what(), "(error_code:", e.error_code(), ")");
				}
			})();
			
			ans["session_key"] = session->session_key;
			ans["session_token"] = session->session_token;
			LOG(DEV, "[link_key:", link->link_key, "] session_key:", session->session_key, ", session_token:", session->session_token);	
		}
		catch (const Exception& e)
		{
			LOG(Log::Logger::LOG_LEVEL_ERR, e.what());
			ans["error_code"] = e.error_code();
		}

		Json::StyledWriter writer;
		std::string str = writer.write(ans);

		Packet::Header header;
		header.msg_id = MSG_ID::MsgID_SvrCli_Connect_Ans;
		header.msg_seq = 1;
		header.length = (uint16_t)(Packet::HEADER_SIZE + str.length()+1);

		std::shared_ptr<Packet> ans_packet = Packet::Create();
		if(nullptr == ans_packet)
		{
			return;
		}
		ans_packet->Write(header, str.c_str(), str.length()+1);
		link->AsyncSend(ans_packet);
	}

	void Recv_Reconnect_Req(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer)
	{
		Json::Value req;
		Json::Value ans;
		ans["error_code"] = 0;
		try 
		{
			std::string json = std::string(buffer->ReadPtr() + Packet::HEADER_SIZE, buffer->Size());
			Json::Reader reader;
			if (false == reader.parse(json, req))
			{
				throw GAMNET_EXCEPTION(ErrorCode::MessageFormatError, "[link_key:", link->link_key, "] message format error(data:", json, ")");
			}

			uint32_t session_key = req["session_key"].asUInt();
			const std::string session_token = req["session_token"].asString();
				
			LOG(DEV, "[link_key:", link->link_key, "] session_key:", session_key, ", session_token:", session_token);	
			const std::shared_ptr<SESSION_T> session = Singleton<LinkManager<SESSION_T>>::GetInstance().FindSession(session_key);
			if (nullptr == session)
			{
				throw GAMNET_EXCEPTION(ErrorCode::InvalidSessionKeyError, "[link_key:", link->link_key, " ] can not find session data for reconnect(session_key:", session_key, ")");
			}

			if(session_token != session->session_token)
			{
				throw GAMNET_EXCEPTION(ErrorCode::InvalidSessionTokenError, "[link_key:", link->link_key, "] invalid session token(expect:", session->session_token, ", receive:", session_token, ")");
			}

			std::shared_ptr<Link> other = std::static_pointer_cast<Link>(session->link);
			if(nullptr != other)
			{
				other->strand.wrap(std::bind(&Link::Close, other, ErrorCode::DuplicateConnectionError))();
			}
			
			link->session = session;

			session->strand.wrap([session, link]() {
				try {
					session->AttachLink(link);
					session->OnAccept();
				}
				catch (const Exception& e)
				{
					LOG(Log::Logger::LOG_LEVEL_ERR, e.what(), "(error_code:", e.error_code(), ")");
				}
			})();
		}
		catch (const Exception& e)
		{
			LOG(Log::Logger::LOG_LEVEL_ERR, e.what());
			ans["error_code"] = e.error_code();
		}

		LOG(DEV, "[link_key:", link->link_key, "] error_code:", ans["error_code"].asUInt());
		Json::StyledWriter writer;
		std::string str = writer.write(ans);

		Packet::Header header;
		header.msg_id = MSG_ID::MsgID_SvrCli_Reconnect_Ans;
		header.msg_seq = std::static_pointer_cast<Tcp::Link>(link)->msg_seq;
		header.length = (uint16_t)(Packet::HEADER_SIZE + str.length()+1);

		std::shared_ptr<Packet> ans_packet = Packet::Create();
		if(nullptr == ans_packet)
		{
			return;
		}
		ans_packet->Write(header, str.c_str(), str.length()+1);
		link->AsyncSend(ans_packet);
	}

	void Recv_HeartBeat_Req(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer)
	{
		Json::Value ans;
		ans["error_code"] = 0;
		ans["msg_seq"] = std::static_pointer_cast<Tcp::Link>(link)->msg_seq;

		Json::StyledWriter writer;
		std::string str = writer.write(ans);

		Packet::Header header;
		header.msg_id = MSG_ID::MsgID_SvrCli_HeartBeat_Ans;
		header.msg_seq = std::static_pointer_cast<Tcp::Link>(link)->msg_seq;
		header.length = (uint16_t)(Packet::HEADER_SIZE + str.length()+1);

		std::shared_ptr<Packet> ans_packet = Packet::Create();
		if(nullptr == ans_packet)
		{
			return;
		}
		ans_packet->Write(header, str.c_str(), str.length()+1);
		link->AsyncSend(ans_packet);
	}

	void Recv_Close_Ntf(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer)
	{
		LOG(DEV, "[link_key:", link->link_key, "]");
		std::shared_ptr<Network::Session> session = link->session;
		link->Close(ErrorCode::Success);
		if(nullptr == session)
		{
			LOG(ERR, "[link_key:", link->link_key, "] null session");
			return;
		}

		session->strand.wrap([session] () {
			try {
				session->OnDestroy();
			}
			catch (const Exception& e)
			{
				LOG(Log::Logger::LOG_LEVEL_ERR, e.what(), "(error_code:", e.error_code(), ")");
			}
		})();

		session_manager.Remove(session->session_key);
	}

	Json::Value State()
	{
		Json::Value root;
		root["name"] = name;

		Json::Value link;
		link["max_count"] = (unsigned int)link_pool.Capacity();
		link["idle_count"] = (unsigned int)link_pool.Available();
		link["active_count"] = (unsigned int)Size();
		root["link"] = link;
		
		Json::Value session;
		session["max_count"] = (unsigned int)session_pool.Capacity();
		session["idle_count"] = (unsigned int)session_pool.Available();
		session["active_count"] = (unsigned int)session_manager.Size();
		root["session"] = session;

#ifdef _DEBUG
		Json::Value message;
		for (auto itr : Singleton<Dispatcher<SESSION_T>>::GetInstance().mapHandlerCallStatistics_)
		{
			Json::Value statistics;
			statistics["msg_id"] = (unsigned int)itr.second->msg_id;
			statistics["msg_name"] = (const char*)itr.second->name;
			statistics["begin_count"] = (unsigned int)itr.second->begin_count;
			statistics["finish_count"] = (unsigned int)itr.second->finish_count;
			statistics["elapsed_time"] = (unsigned int)itr.second->elapsed_time;
			if (0 == itr.second->elapsed_time || 0 == itr.second->finish_count)
			{
				statistics["average_time"] = 0;
			}
			else
			{
				statistics["average_time"] = (int)(itr.second->elapsed_time / itr.second->finish_count);
			}
			message.append(statistics);
		}
		root["message"] = message;
#endif
		return root;
	}
};

}}}
#endif /* LISTENER_H_ */
