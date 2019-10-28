#pragma once
#include <muduo/net/TcpServer.h>

#include <unordered_set>

#include <boost/circular_buffer.hpp>

class EchoServer {
public:
	EchoServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr, int idleSeconds);
	void start() {
		server_.start();
	}

private:
	void onConnection(const std::shared_ptr<muduo::net::TcpConnection>& conn);
	void onMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn,
				   muduo::net::Buffer* buf,
				   muduo::Timestamp time);
	void onTimer();
	void dumpConnectionBuckets() const;

	struct Entry :public muduo::copyable {
		explicit Entry(const std::weak_ptr<muduo::net::TcpConnection>& weakConn)
			:weakConn_(weakConn) {}
		~Entry() {
			// 如果不是空悬指针 升为shared_ptr
			std::shared_ptr<muduo::net::TcpConnection> conn = weakConn_.lock();
			if (conn) {
				conn->shutdown();
			}
		}
		std::weak_ptr<muduo::net::TcpConnection> weakConn_;
	};
private:
	muduo::net::TcpServer server_;
	boost::circular_buffer<std::unordered_set<std::shared_ptr<Entry>>> connectionBuckets_;
};