#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>

#include <iostream>
#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class ChatClient : noncopyable {
public:
	ChatClient(EventLoop* loop, const InetAddress& serverAddr) 
		:client_(loop,serverAddr,"chat Client"),
		codec_([](const std::shared_ptr<TcpConnection>&,const string& message,Timestamp) {
				printf("<<< %s\n", message.data());
			}){
		// constractor start
		auto connectionF = [this](const std::shared_ptr<TcpConnection>& conn) {
			LOG_INFO << conn->localAddress().toIpPort() << " -> "
				<< conn->peerAddress().toIpPort() << " is "
				<< (conn->connected() ? "UP" : "DOWN");
			MutexLockGuard lock(mutex_);
			if (conn->connected()) {
				connection_ = conn;
			}
			else {
				connection_.reset();
			}
		};
		client_.setConnectionCallback(connectionF);
		client_.setMessageCallback(std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
	}


	auto connect() -> void{
		client_.connect();
	}

	auto disconnect() -> void {
		client_.disconnect();
	}

	auto write(const StringPiece& message) -> void{
		MutexLockGuard lock(mutex_);
		if (connection_) {
			codec_.send(get_pointer(connection_), message);
		}
	}

private:

private:
	TcpClient client_;
	LengthHeaderCodec codec_;
	MutexLock mutex_;
	std::shared_ptr<TcpConnection> connection_ GUARDED_BY(mutex_);
};

auto main(int argc,char* argv[]) -> int{
	LOG_INFO << "pid=" << getpid();
	if (argc > 2) {
		EventLoopThread loopThread;
		auto port = static_cast<uint16_t>(atoi(argv[2]));
		InetAddress serverAddr(argv[1], port);
		ChatClient client(loopThread.startLoop(), serverAddr);
		client.connect();
		std::string line;
		while (std::getline(std::cin, line)) {
			client.write(line);
		}
		client.disconnect();
		CurrentThread::sleepUsec(1000 * 1000);
	}
	else {
		printf("Usage: %s host_ip port\n", argv[0]);
	}
	return 0;
}