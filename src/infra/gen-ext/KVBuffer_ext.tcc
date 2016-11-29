#pragma once

#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen/gen-cpp2/commontypes_constants.h>
#include <folly/Conv.h>
#include <folly/futures/Future.h>
#include <infra/Serializer.tcc>

namespace infra {

#if 0
template <class T>
struct KVBufferExt {
    KVBufferExt(const T& data) {
        data_ = data;
    }

    KVBufferExt(const KVBuffer &kvb)
    {
        props_ = kvb.props;
        data_ = deserializeFromThriftJson<T>(kvb.data, ""); 
    }

    const T& data() const {
        return data_;
    }

    T& data() {
        return data_;
    }

    template <class PropT>
    PropT getProp(const std::string &key) {
        return folly::to<PropT>(props_.at(key));
    }

    template <class PropT>
    void setProp(const std::string &key, const PropT &val) {
        props_[key] = folly::to<PropT>(val);
    }

    KVBuffer toKVBuffer() {
        KVBuffer kvb;
        kvb.props = props_;
        kvb.data = serializeToThriftJson<T>(data_, "");
        return kvb;
    }

    static KVBufferExt fromKVBufferThriftJson(const std::string &tJson) {
        KVBuffer kvb = deserializeFromThriftJson<KVBuffer>(tJson, "");
        return KVBufferExt(kvb);
    }

    std::string toKVBufferThriftJson() {
        auto kvb = toKVBuffer();
        std::string tJson = serializeToThriftJson<KVBuffer>(kvb, "");
        return tJson;
    }

 private:
    T                                       data_;
    std::map<std::string, std::string>      props_;
};
#endif

template <class T>
void setProp(KVBuffer &kvb, const std::string &key, const T &val)
{
    kvb.props[key] = folly::to<std::string>(val);
}

template <class T>
T getProp(const KVBuffer &kvb, const std::string &key)
{
    return folly::to<T>(kvb.props.at(key));
}

inline void setVersion(KVBuffer &kvb, int64_t version)
{
    setProp<int64_t>(kvb, commontypes_constants::KEY_VERSION(), version);
}

inline int64_t getVersion(const KVBuffer &kvb)
{
    return getProp<int64_t>(kvb, commontypes_constants::KEY_VERSION());
}

inline void setType(KVBuffer &kvb, const std::string &type)
{
    setProp<std::string>(kvb, commontypes_constants::KEY_TYPE(), type);
}

inline std::string getType(const KVBuffer &kvb)
{
    return getProp<std::string>(kvb, commontypes_constants::KEY_TYPE());
}

inline void setId(KVBuffer &kvb, const std::string &id)
{
    setProp<std::string>(kvb, commontypes_constants::KEY_ID(), id);
}

inline std::string getId(const KVBuffer &kvb)
{
    return getProp<std::string>(kvb, commontypes_constants::KEY_ID());
}

template <class T>
inline void setAsThriftJsonPayload(KVBuffer &kvb, const T &payload)
{
    folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
    apache::thrift::JSONSerializer::serialize<>(payload, &queue);
    kvb.payload = queue.move();
}

template <class T>
inline T getFromThriftJsonPayload(const KVBuffer &kvb)
{
    return apache::thrift::JSONSerializer::deserialize<T>(kvb.get_payload().get());
}

template <class T>
inline void setAsBinaryPayload(KVBuffer &kvb, const T &payload)
{
    folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
    apache::thrift::BinarySerializer::serialize<>(payload, &queue);
    kvb.payload = queue.move();
}

template <class T>
inline T getFromBinaryPayload(const KVBuffer &kvb)
{
    return apache::thrift::BinarySerializer::deserialize<T>(kvb.get_payload().get());
}

inline std::string toString(const folly::IOBuf& buf) {
    std::string result;
    result.reserve(buf.computeChainDataLength());
    for (auto& b : buf) {
        result.append(reinterpret_cast<const char*>(b.data()), b.size());
    }
    return result;
}


}  // namespace infra
