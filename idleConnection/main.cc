#include "echo.h"
#include <stdio.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

using namespace muduo;
using namespace muduo::net;

auto main(int argc, char* argv[]) ->int {
	EventLoop loop;
	InetAddress listenAddr(2007);
	auto idleSeconds = 10;
	LOG_INFO << "pid=" << getpid() << " , idle seconds = " << idleSeconds;
	EchoServer server(&loop, listenAddr, idleSeconds);
	server.start();
	loop.loop();

	return 0;
}