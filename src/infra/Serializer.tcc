#pragma once
#include <util/Log.h>
#include <memory>
#include <thrift/lib/cpp2/protocol/Serializer.h>

namespace infra {

template <class PayloadT>
std::string serializeToThriftJson(const PayloadT &payload,
                                  const std::string &logCtx)
{
    std::string ret;
    apache::thrift::JSONSerializer::serialize(payload, &ret);
    return ret;
}

template<class PayloadT>
PayloadT deserializeFromThriftJson(const std::string &payloadBuf,
                                   const std::string &logCtx)
{
    PayloadT ret;
    apache::thrift::JSONSerializer::deserialize(payloadBuf, ret);
    return ret;
}

template <class PayloadT>
std::string toJsonString(const PayloadT &msg)
{
    std::string ret;
    apache::thrift::JSONSerializer::serialize(msg, &ret);
    return ret;
}

} // namespace fds
