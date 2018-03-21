#include "LinkManager.h"
#include "Dispatcher.h"
#include "Request.h"
#include "../../Library/String.h"
#include <curl/curl.h>


namespace Gamnet { namespace Network { namespace Http {

LinkManager::LinkManager() : link_pool(65535, LinkFactory(this))
{
	_name = "Gamnet::Network::Http::LinkManager";
}

LinkManager::~LinkManager() 
{
}

std::shared_ptr<Network::Link> LinkManager::Create() 
{
	std::shared_ptr<Link> link = link_pool.Create();
	if(nullptr == link)
	{
		LOG(GAMNET_ERR, "can not create 'Tcp::Link' instance");
		return nullptr;
	}

	if (nullptr == link->read_buffer)
	{
		LOG(GAMNET_ERR, "can not create read buffer");
		return nullptr;
	}

	if (nullptr == link->recv_buffer)
	{
		LOG(GAMNET_ERR, "can not create recv packet");
		return nullptr;
	}

	const std::shared_ptr<Session> session = std::shared_ptr<Session>(new Session());
	if (nullptr == session)
	{
		LOG(GAMNET_ERR, "create session instance fail(link_key:", link->link_key, ")");
		return nullptr;
	}

	link->session = session;
	session->link = link;
	session->remote_address = &(link->remote_address);

	return link;
}

void LinkManager::OnAccept(const std::shared_ptr<Network::Link>& link)
{
	std::shared_ptr<Network::Session> session = link->session;
	session->OnAccept();
}

void LinkManager::OnClose(const std::shared_ptr<Network::Link>& link, int reason)
{
	if (nullptr != link->session)
	{
		link->session->OnClose(reason);
	}
	Network::LinkManager::OnClose(link, reason);
}

void LinkManager::OnRecvMsg(const std::shared_ptr<Network::Link>& link, const std::shared_ptr<Buffer>& buffer)
{
}

}}}
