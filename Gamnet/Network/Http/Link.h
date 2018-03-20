#ifndef GAMNET_NETWORK_HTTP_LINK_H_
#define GAMNET_NETWORK_HTTP_LINK_H_

#include "../Link.h"

namespace Gamnet { namespace Network { namespace Http {
	class Link : public Network::Link {
	public :
		struct Init
		{
			Link* operator() (Link* link)
			{
				Network::Link::Init init;
				init(link);
				link->recv_buffer = Buffer::Create();
				return link;
			}
		};

		std::shared_ptr<Buffer> recv_buffer;

		virtual void OnRecvMsg() override;
	};
}}}
#endif