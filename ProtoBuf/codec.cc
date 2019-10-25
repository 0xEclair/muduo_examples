#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>
#include "google-inl.h"

#include <google/protobuf/descriptor.h>

#include <zlib.h>

using namespace muduo;
using namespace muduo::net;

namespace {
	const string kNoErrorStr = "NoError";
	const string kInvalidLengthStr = "InvalidLength";
	const string kCheckSumErrorStr = "CheckSumError";
	const string kInvalidNameLenStr = "InvalidNameLen";
	const string kUnknownMessageTypeStr = "UnKnownMessageType";
	const string kParseErrorStr = "ParseError";
	const string kUnknownErrorStr = "UnknownError";
}

const muduo::string& ProtobufCodec::errorCodeToString(ErrorCode errorCode){
	switch (errorCode){
	case kNoError:
		return kNoErrorStr;
	case kInvalidLength:
		return kInvalidLengthStr;
	case kCheckSumError:
		return kCheckSumErrorStr;
	case kInvalidNameLen:
		return kInvalidNameLenStr;
	case kUnknownMessageType:
		return kUnknownMessageTypeStr;
	case kParseError:
		return kParseErrorStr;
	default:
		return kUnknownErrorStr;
	}
}

void ProtobufCodec::fillEmptyBuffer(Buffer* buf, const Message& message) {
	// buf->retrieveAll();
	const auto& type_name = message.GetTypeName();
	auto name_len = static_cast<int32_t>(type_name.size() + 1);
	buf->appendInt32(name_len);
	buf->append(type_name.c_str(), name_len);

	GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);

	int byte_size = message.ByteSize();
	buf->ensureWritableBytes(byte_size);

	auto* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
	auto* end = message.SerializeWithCachedSizesToArray(start);
	if (end - start != byte_size) {
		ByteSizeConsistencyError(byte_size, message.ByteSize(), static_cast<int>(end - start));
	}
	buf->hasWritten(byte_size);

	auto checkSum = static_cast<int32_t>(
		::adler32(1, 
				  reinterpret_cast<const Bytef*>(buf->peek()), 
				  static_cast<int>(buf->readableBytes())));

	buf->appendInt32(checkSum);
	assert(buf->readableBytes() == sizeof name_len + name_len + byte_size + sizeof checkSum);
	auto len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
	buf->prepend(&len, sizeof len);
}

void ProtobufCodec::defaultErrorCallback(const std::shared_ptr<muduo::net::TcpConnection>& conn, muduo::net::Buffer*, muduo::Timestamp, ErrorCode errorCode){
	LOG_ERROR << "ProtobufCodec::defaultErrorCallback - " << errorCodeToString(errorCode);
	if (conn && conn->connected()) {
		conn->shutdown();
	}
}

auto asInt32(const char* buf) ->int32_t {
	int32_t be32 = 0;
	::memcpy(&be32, buf, sizeof(be32));
	return sockets::networkToHost32(be32);
}

auto ProtobufCodec::onMessage(const std::shared_ptr<muduo::net::TcpConnection>& conn, muduo::net::Buffer* buf, muduo::Timestamp receiveTime) -> void {
	while (buf->readableBytes() >= kMinMessageLen + kHeaderLen) {
		const auto len = buf->peekInt32();
		if (len > kMaxMessageLen || len < kMinMessageLen) {
			errorCallback_(conn, buf, receiveTime, kInvalidLength);
			break;
		}
		else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen)) {
			auto errorCode = kNoError;
			auto message = parse(buf->peek() + kHeaderLen, len, &errorCode);
			if (errorCode == kNoError && message) {
				messageCallback_(conn, message, receiveTime);
				buf->retrieve(kHeaderLen + len);
			}
			else {
				errorCallback_(conn, buf, receiveTime, errorCode);
				break;
			}
		}
		else {
			break;
		}
	}
}

Message* ProtobufCodec::createMessage(const std::string& type_name){
	Message* message = nullptr;
	const auto* descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
	if (descriptor) {
		const auto* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
		if (prototype) {
			message = prototype->New();
		}
	}
	return message;
}

std::shared_ptr<Message> ProtobufCodec::parse(const char* buf, int len, ErrorCode* error){
	std::shared_ptr<Message> message;

	// check sum
	auto expected_check_sum = asInt32(buf + len - kHeaderLen);
	auto check_sum = static_cast<int32_t>(
		::adler32(1,
			reinterpret_cast<const Bytef*>(buf),
			static_cast<int>(len - kHeaderLen)));
	if (check_sum == expected_check_sum) {
		// get message type name
		auto name_len = asInt32(buf);
		if (name_len >= 2 && name_len <= len - 2 * kHeaderLen) {
			std::string type_name(buf + kHeaderLen, buf + kHeaderLen + name_len - 1);
			message.reset(createMessage(type_name));
			if (message) {
				const auto* data = buf + kHeaderLen + name_len;
				auto data_len = len - name_len - 2 * kHeaderLen;
				if (message->ParseFromArray(data, data_len)) {
					*error = kNoError;
				}
				else {
					*error = kParseError;
				}
			}
			else {
				*error = kUnknownMessageType;
			}
		}
		else {
			*error = kInvalidNameLen;
		}
	}
	else {
		*error = kCheckSumError;
	}

	return message;
}