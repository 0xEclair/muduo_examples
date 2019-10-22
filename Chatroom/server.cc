#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <set>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class ChatServer :noncopyable {
public:
	ChatServer(EventLoop* loop,const InetAddress& listenAddr)
		:server_(loop, listenAddr, "chatserver"),
		 codec_(
			[this](const std::shared_ptr<TcpConnection>&, const string& message, Timestamp) {
				for (const auto& it : connections_) {
					codec_.send(get_pointer(it), message);
			}})
	{
		server_.setConnectionCallback([this](const std::shared_ptr<TcpConnection>& conn) {
			LOG_INFO << conn->localAddress().toIpPort() << " -> "
				<< conn->peerAddress().toIpPort() << " is "
				<< (conn->connected() ? "UP" : "DOWN");
			if (conn->connected()) {
				connections_.insert(conn);
			}
			else {
				connections_.erase(conn);
			}
			});
		server_.setMessageCallback(
			std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3)
		);
	}


	void start() {
		server_.start();
	}
private:

private:
	using ConnectionList= std::set<std::shared_ptr<muduo::net::TcpConnection>>;
	TcpServer server_;
	LengthHeaderCodec codec_;
	ConnectionList connections_;
};

auto main(int argc,char* argv[]) -> int{
	LOG_INFO << "pid=" << getpid();
	if (argc > 1) {
		EventLoop loop;
		auto port = static_cast<uint16_t>(atoi(argv[1]));
		InetAddress serverAddr(port);
		ChatServer server(&loop, serverAddr);
		server.start();
		loop.loop();
	}
	else {
		printf("usage:%s port\n", argv[0]);
	}
	return 0;
}
