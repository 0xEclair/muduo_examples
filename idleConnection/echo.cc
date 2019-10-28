#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <assert.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EchoServer::EchoServer(EventLoop* loop, const InetAddress& listenAddr, int idleSeconds) 
	:server_(loop,listenAddr,"EchoServer"),connectionBuckets_(idleSeconds){
	server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, _1));
	server_.setMessageCallback(std::bind(&EchoServer::onMessage,this,_1,_2,_3));
	loop->runEvery(1.0, std::bind(&EchoServer::onTimer, this));
	connectionBuckets_.resize(idleSeconds);
	dumpConnectionBuckets();
}

void EchoServer::onConnection(const std::shared_ptr<muduo::net::TcpConnection>& conn){
	LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
		<< conn->localAddress().toIpPort() << "is"
		<< (conn->connected() ? "UP" : "DOWN");

	if (conn->connected()) {
		std::shared_ptr<Entry> entry(new Entry(conn));
		connectionBuckets_.back().insert(entry);
		dumpConnectionBuckets();
		std::weak_ptr<Entry> weakEntry(entry);
		conn->setContext(weakEntry);
	}
	else {
		assert(!conn->getContext().empty());
		std::weak_ptr<Entry> weakEntry(boost::any_cast<std::weak_ptr<Entry>>(conn->getContext()));
		LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
	}
}

void EchoServer::onMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn, muduo::net::Buffer* buf, muduo::Timestamp time){
	string msg(buf->retrieveAllAsString());
	LOG_INFO << conn->name() << " echo " << msg.size()
		<< " bytes at " << time.toString();
	conn->send(msg);

	assert(!conn->getContext().empty());
	std::weak_ptr<Entry> weakEntry(boost::any_cast<std::weak_ptr<Entry>>(conn->getContext()));
	std::shared_ptr<Entry> entry(weakEntry.lock());
	if (entry) {
		connectionBuckets_.back().insert(entry);
		dumpConnectionBuckets();
	}
}

void EchoServer::onTimer() {
	connectionBuckets_.push_back(std::unordered_set<std::shared_ptr<Entry>>());
	dumpConnectionBuckets();
}

void EchoServer::dumpConnectionBuckets() const{
	LOG_INFO << "size = " << connectionBuckets_.size();
	auto idx = 0;
	for (const auto& bucket : connectionBuckets_) {
		printf("[%d] len = %zd : ", idx++, bucket.size());
		for (const auto& it : bucket) {
			bool connectionDead = it->weakConn_.expired();
			printf("%p(%ld)%s,", get_pointer(it), it.use_count(), connectionDead ? "DEAD" : "");
		}
		puts("");
		idx;
	}
}