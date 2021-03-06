#ifndef GAMNET_NETWORK_ROUTER_H_
#define GAMNET_NETWORK_ROUTER_H_

#include "../Tcp/Tcp.h"
#include "RouterCaster.h"
#include "Dispatcher.h"
#include "LinkManager.h"

namespace Gamnet { namespace Network { namespace Router {
	const Address& GetRouterAddress();

	void Listen(const char* service_name, int port, const std::function<void(const Address& addr)>& onAccept = [](const Address&){}, const std::function<void(const Address& addr)>& onClose = [](const Address&) {});
	void Connect(const char* host, int port, int timeout, const std::function<void(const Address& addr)>& onConnect = [](const Address&) {}, const std::function<void(const Address& addr)>& onClose = [](const Address&) {});
	
	template <class FUNC, class FACTORY>
	bool RegisterHandler(unsigned int msg_id, FUNC func, FACTORY factory)
	{
		return Singleton<Dispatcher>::GetInstance().RegisterHandler(msg_id, func, factory);
	}

	template <class MSG>
	bool SendMsg(const std::shared_ptr<Network::Tcp::Session>& session, const Address& addr, const MSG& msg)
	{
		uint32_t msg_seq = 0;
		if(nullptr != session)
		{
			std::shared_ptr<Network::Tcp::Link> link = std::static_pointer_cast<Network::Tcp::Link>(session->link);
			if(nullptr == link)
			{
				throw GAMNET_EXCEPTION(ErrorCode::NullPointerError, "invalid link(session_key:", session->session_key, ", msg_id:", MSG::MSG_ID, ")");
			}

			msg_seq = link->msg_seq;
		}

		std::shared_ptr<Network::Tcp::Packet> packet = Network::Tcp::Packet::Create();
		if(nullptr == packet)
		{
			throw GAMNET_EXCEPTION(ErrorCode::NullPointerError, "fail to create packet instance(session_key:", session->session_key, ", msg_id:", MSG::MSG_ID, ")");
		}

		if(false == packet->Write(msg_seq, msg))
		{
			throw GAMNET_EXCEPTION(ErrorCode::MessageFormatError, "fail to serialize message(session_key:", session->session_key, ", msg_id:", MSG::MSG_ID, ")");
		}

		return Singleton<RouterCaster>::GetInstance().SendMsg(session, addr, packet->ReadPtr(), (int)packet->Size());
	}

	template <class MSG>
	bool SendMsg(const Address& addr, const MSG& msg)
	{
		return SendMsg(nullptr, addr, msg);
	}
}}}

#define GAMNET_BIND_ROUTER_HANDLER(message_type, class_type, func, policy) \
	static bool Router_##message_type##_##func = Gamnet::Network::Router::RegisterHandler( \
		message_type::MSG_ID, \
		&class_type::func, \
		new Gamnet::Network::policy<class_type>() \
	)

#endif /* ROUTER_H_ */
