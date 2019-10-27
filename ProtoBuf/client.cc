#include "codec.h"
#include "dispatcher.h"
#include "query.pb.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

google::protobuf::Message* messageToSend;

class QueryClient :noncopyable {
public:
	QueryClient(EventLoop* loop, const InetAddress& serverAddr)
		:loop_(loop), client_(loop, serverAddr, "QueryClient"),
		dispatcher_(
			[](const std::shared_ptr<TcpConnection>& conn,
				const std::shared_ptr<Message>& message,
				Timestamp) {
					LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
			}),
		codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)) {

		dispatcher_.registerMessageCallback<Query>(
			[](const std::shared_ptr<TcpConnection>& conn,
				const std::shared_ptr<Query>& message,
				Timestamp) {
					LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
			});

		dispatcher_.registerMessageCallback<Answer>(
			[](const std::shared_ptr<TcpConnection>& conn,
				const std::shared_ptr<Answer>& message,
				Timestamp) {
					LOG_INFO << "onAnswer:\n" << message->GetTypeName() << message->DebugString();
			});
		dispatcher_.registerMessageCallback<Empty>(
			[](const std::shared_ptr<TcpConnection>& conn,
				const std::shared_ptr<Empty>& message,
				Timestamp) {
					LOG_INFO << "onEmpty:" << message->GetTypeName();

			});

		client_.setConnectionCallback(
			[this](const std::shared_ptr<TcpConnection>& conn) {
				LOG_INFO << conn->localAddress().toIpPort() << " -> "
					<< conn->peerAddress().toIpPort() << " is "
					<< (conn->connected() ? "UP" : "DOWN");
				if (conn->connected()) {
					codec_.send(conn, *messageToSend);
				}
				else {
					loop_->quit();
				}
			}
		);
		client_.setMessageCallback(std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
	}

	void connect() {
		client_.connect();
	}
private:

private:
	EventLoop* loop_;
	TcpClient client_;
	ProtobufDispatcher dispatcher_;
	ProtobufCodec codec_;
};

int main(int argc, char* argv[])
{
	LOG_INFO << "pid = " << getpid();
	if (argc > 2)
	{
		EventLoop loop;
		uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
		InetAddress serverAddr(argv[1], port);

		muduo::Query query;
		query.set_id(1);
		query.set_questioner("Chen Shuo");
		query.add_question("Running?");
		muduo::Empty empty;
		messageToSend = &query;

		if (argc > 3 && argv[3][0] == 'e')
		{
			messageToSend = &empty;
		}

		QueryClient client(&loop, serverAddr);
		client.connect();
		loop.loop();
	}
	else
	{
		printf("Usage: %s host_ip port [q|e]\n", argv[0]);
	}

	return 0;
}

