#pragma once
#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>

#include <google/protobuf/message.h>
using google::protobuf::Message;
class ProtobufCodec : muduo::noncopyable {
public:
	enum ErrorCode {
		kNoError=0,
		kInvalidLength,
		kCheckSumError,
		kInvalidNameLen,
		kUnknownMessageType,
		kParseError,
	};

	using ProtobufMessageCb=std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
											   const std::shared_ptr<Message>&,
											   muduo::Timestamp)>;

	using ErrorCallback=std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
										   muduo::net::Buffer*,
										   muduo::Timestamp,
										   ErrorCode)>;

	explicit ProtobufCodec(const ProtobufMessageCb& messageCb)
		:messageCallback_(messageCb),errorCallback_(defaultErrorCallback){

	}

	ProtobufCodec(const ProtobufMessageCb& messageCb, const ErrorCallback& errorCb)
		:messageCallback_(messageCb), errorCallback_(errorCb) {

	}

	auto onMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn,
				   muduo::net::Buffer* buf,
				   muduo::Timestamp receiveTime) -> void;

	auto send(const std::shared_ptr<muduo::net::TcpConnection>& conn, const Message& message) -> void {
		muduo::net::Buffer buf;
		fillEmptyBuffer(&buf, message);
		conn->send(&buf);
	}

	static const muduo::string& errorCodeToString(ErrorCode errorCode);
	static void fillEmptyBuffer(muduo::net::Buffer* buf, const Message& message);
	static Message* createMessage(const std::string& type_name);
	static std::shared_ptr<Message> parse(const char* buf, int len, ErrorCode* errorCode);

private:
	static void defaultErrorCallback(const std::shared_ptr<muduo::net::TcpConnection>&,
		muduo::net::Buffer*,
		muduo::Timestamp,
		ErrorCode);
private:

	ProtobufMessageCb messageCallback_;
	ErrorCallback errorCallback_;

	const static int kHeaderLen = sizeof(int32_t);
	const static int kMinMessageLen = 2 * kHeaderLen + 2;
	const static int kMaxMessageLen = 64 * 1024 * 1024;
};