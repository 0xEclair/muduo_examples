#pragma once

#include <muduo/base/noncopyable.h>
#include <muduo/net/Callbacks.h>

#include <google/protobuf/message.h>

#include <unordered_map>
#include <type_traits>

using google::protobuf::Message;

class Callback : muduo::noncopyable {
public:
	virtual ~Callback() = default;
	virtual void onMessage(const std::shared_ptr<muduo::net::TcpConnection>&,
						   const std::shared_ptr<Message>& message,
						   muduo::Timestamp) const = 0;
};

template <typename T>
class CallbackT : public Callback {
	static_assert(std::is_base_of<Message, T>::value, "T must be derived from gpb::Message.");
public:
	using ProtobufMessageTCb= std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
											    const std::shared_ptr<T>& message,
											    muduo::Timestamp)> ;

	CallbackT(const ProtobufMessageTCb& callback)
		:callback_(callback){
	
	}

	void onMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn,
				   const std::shared_ptr<Message>& message,
				   muduo::Timestamp receiveTime) const override {
		std::shared_ptr<T> concreate = muduo::down_pointer_cast<T>(message);
		assert(concreate != nullptr);
		callback_(conn, concreate, receiveTime);
	}

private:

private:
	ProtobufMessageTCb callback_;
};

class ProtobufDispatcher {
public:
	using ProtobufMessageCb = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
												 const std::shared_ptr<Message>& message,
												 muduo::Timestamp)>;

	explicit ProtobufDispatcher(const ProtobufMessageCb& defaultCb)
		:defaultCallback_(defaultCb) {

	}

	void onProtobufMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn,
						   const std::shared_ptr<Message>& message,
						   muduo::Timestamp receiveTime) const {
		auto it = callbacks_.find(message->GetDescriptor());
		if (it != callbacks_.end()) {
			it->second->onMessage(conn, message, receiveTime);
		}
		else {
			defaultCallback_(conn, message, receiveTime);
		}
	}

	template<typename T>
	void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageTCb& callback) {
		// 关于为什么要用new 因为实际上保存的是CallbackT 但是map中的类型是Callback 目的是多态
		std::shared_ptr<CallbackT<T>> pd(new CallbackT<T>(callback));
		callbacks_[T::descriptor()] = pd;
	}

private:
	std::unordered_map<const google::protobuf::Descriptor*, std::shared_ptr<Callback>> callbacks_;
	ProtobufMessageCb defaultCallback_;
};