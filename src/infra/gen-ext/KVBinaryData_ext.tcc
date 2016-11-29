#pragma once

#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen/gen-cpp2/commontypes_constants.h>
#include <folly/Conv.h>
#include <folly/futures/Future.h>
#include <infra/Serializer.tcc>

namespace infra {

#if 0
template <class T>
struct KVBinaryDataExt {
    KVBinaryDataExt(const T& data) {
        data_ = data;
    }

    KVBinaryDataExt(const KVBinaryData &kvb)
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

    KVBinaryData toKVBinaryData() {
        KVBinaryData kvb;
        kvb.props = props_;
        kvb.data = serializeToThriftJson<T>(data_, "");
        return kvb;
    }

    static KVBinaryDataExt fromKVBinaryDataThriftJson(const std::string &tJson) {
        KVBinaryData kvb = deserializeFromThriftJson<KVBinaryData>(tJson, "");
        return KVBinaryDataExt(kvb);
    }

    std::string toKVBinaryDataThriftJson() {
        auto kvb = toKVBinaryData();
        std::string tJson = serializeToThriftJson<KVBinaryData>(kvb, "");
        return tJson;
    }

 private:
    T                                       data_;
    std::map<std::string, std::string>      props_;
};
#endif

template <class T>
void setProp(KVBinaryData &kvb, const std::string &key, const T &val)
{
    kvb.props[key] = folly::to<std::string>(val);
}

template <class T>
T getProp(const KVBinaryData &kvb, const std::string &key)
{
    return folly::to<T>(kvb.props.at(key));
}

inline void setVersion(KVBinaryData &kvb, int64_t version)
{
    setProp<int64_t>(kvb, commontypes_constants::KEY_VERSION(), version);
}

inline int64_t getVersion(const KVBinaryData &kvb)
{
    return getProp<int64_t>(kvb, commontypes_constants::KEY_VERSION());
}

inline void setType(KVBinaryData &kvb, const std::string &type)
{
    setProp<std::string>(kvb, commontypes_constants::KEY_TYPE(), type);
}

inline std::string getType(const KVBinaryData &kvb)
{
    return getProp<std::string>(kvb, commontypes_constants::KEY_TYPE());
}

inline void setId(KVBinaryData &kvb, const std::string &id)
{
    setProp<std::string>(kvb, commontypes_constants::KEY_ID(), id);
}

inline std::string getId(const KVBinaryData &kvb)
{
    return getProp<std::string>(kvb, commontypes_constants::KEY_ID());
}

template <class T>
inline void setAsThriftJsonPayload(KVBinaryData &kvb, const T &payload)
{
    folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
    apache::thrift::JSONSerializer::serialize<>(payload, &queue);
    kvb.payload = queue.move();
}

template <class T>
inline T getFromThriftJsonPayload(const KVBinaryData &kvb)
{
    return apache::thrift::JSONSerializer::deserialize<T>(kvb.get_payload().get());
}

template <class T>
inline void setAsBinaryPayload(KVBinaryData &kvb, const T &payload)
{
    folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
    apache::thrift::BinarySerializer::serialize<>(payload, &queue);
    kvb.payload = queue.move();
}

template <class T>
inline T getFromBinaryPayload(const KVBinaryData &kvb)
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
