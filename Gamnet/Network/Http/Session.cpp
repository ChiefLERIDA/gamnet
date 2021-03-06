/*
 * Session.cpp
 *
 *  Created on: Aug 8, 2014
 *      Author: kukuta
 */

#include "Session.h"
#include "../../Library/String.h"
#include <curl/curl.h>
namespace Gamnet { namespace Network { namespace Http {

Session::Session() {
}

Session::~Session() {
}

void Session::Send(const Response& res)
{
	std::string response;
	response += Format("HTTP/1.1 ", res.error_code, " ", GetErrorStr(res.error_code), "\r\n");
	response += Format("\r\n");
	response += res.context;
	response += Format("\r\n\r\n");
	AsyncSend(response.c_str(), response.length());
}

}}}
