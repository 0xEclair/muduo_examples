#include "codec.h"
#include "dispatcher.h"
#include "query.pb.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

class QueryServer : noncopyable {
public:
	QueryServer(EventLoop* loop, const InetAddress& listenAddr)
		:server_(loop, listenAddr, "QueryServer"),
		dispatcher_([](const std::shared_ptr<TcpConnection>& conn,
			const std::shared_ptr<Message>& message,
			Timestamp) {
				LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
				conn->shutdown();
			}),
		codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)) {

		dispatcher_.registerMessageCallback<muduo::Query>
			([this](const std::shared_ptr<TcpConnection>& conn,
					const std::shared_ptr<muduo::Query>& message,
					Timestamp) {
						LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
						Answer answer;
						answer.set_id(1);
						answer.set_questioner("CHEN SHUO");
						answer.set_answerer("blog.csdn.net/Solstice");
						answer.add_solution("Jump!");
						answer.add_solution("Win!");
						codec_.send(conn, *message);
				});
		dispatcher_.registerMessageCallback<muduo::Answer>(
			[](const std::shared_ptr<TcpConnection>& conn,
				const std::shared_ptr<Answer>& message,
				Timestamp) {
					LOG_INFO << "onAnswer: " << message->GetTypeName();
					conn->shutdown();
			});
		server_.setConnectionCallback([](const std::shared_ptr<TcpConnection>& conn) {
			LOG_INFO << conn->localAddress().toIpPort() << " -> "
				<< conn->peerAddress().toIpPort() << " is "
				<< (conn->connected() ? "UP" : "DOWN");
			});
		server_.setMessageCallback(std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
	}

	void start() {
		server_.start();
	}

private:

private:
	TcpServer server_;
	ProtobufDispatcher dispatcher_;
	ProtobufCodec codec_;
};

auto main()->int {
	LOG_INFO << "pid = " << getpid();
	EventLoop loop;
	InetAddress serverAddr(2007);
	QueryServer server(&loop, serverAddr);
	server.start();
	loop.loop();
	return 0;
}