# gamnet
	even though I originally intened to "Gamenet"..beause of typing error, it became "gamnet"..;;
	boost asio based server framework using c++ 11
	
# Features
	client connection managing(automatically create and delete, idle connection timeout)
	socket buffer managing
	server to server communication by logical address(not ip and port)
	support structured message
	support message serialize/desreialize compiler
	support logging library
	support timer, thread pool
	base64, md5
	unit test tool
	
# Quick Example	
/**
    1. include 'Gamnet.h' and using namespace 
    may make you a little bit comfortable for writing code
*/
#include <Gamnet.h>
using namespace Gamnet;

/**
    2. Write Your Own Session class
    It handles network event (OnAccept, OnClose, OnError, Send, Recv..) and 
    store Data 	
