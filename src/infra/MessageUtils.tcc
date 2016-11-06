#pragma once
#include <string>
#include <memory>
#include <folly/futures/Future.h>
#include <infra/ConnectionCache.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <infra/gen/gen-cpp2/ServiceApi.h>
#include <infra/typestr.h>

namespace infra {

template <class ReqT, class RespT>
folly::Future<RespT> sendKVBMessage(ConnectionCache *connMgr,
                                    const std::string &id,
                                    const ReqT& req)
{
    auto reqKvb = std::make_unique<KVBinaryData>();
    setType(*reqKvb, typeStr<ReqT>());
    reqKvb->data = serializeToThriftJson<ReqT>(req, "");
    auto f = 
        connMgr
        ->getAsyncClient<ServiceApiAsyncClient>(id)
        .then([reqKvb=std::move(reqKvb)](const std::shared_ptr<ServiceApiAsyncClient>& client) {
            return client 
                    ->future_handleKVBMessage(*reqKvb)
                    .then([](const KVBinaryData &respKvb) {
                          return deserializeThriftJsonData<RespT>(respKvb,"");
                    });
        });
    return f;
}

template <class ReqT>
folly::Future<folly::Unit> sendKVBMessage(ConnectionCache *connMgr,
                                    const std::string &id,
                                    const ReqT& req)
{
    auto reqKvb = std::make_unique<KVBinaryData>();
    setType(*reqKvb, typeStr<ReqT>());
    reqKvb->data = serializeToThriftJson<ReqT>(req, "");
    auto f = 
        connMgr
        ->getAsyncClient<ServiceApiAsyncClient>(id)
        .then([reqKvb=std::move(reqKvb)](const std::shared_ptr<ServiceApiAsyncClient>& client) {
            return client 
                    ->future_handleKVBMessage(*reqKvb)
                    .then([](const KVBinaryData &) {
                          return;
                    });
        });
    return f;
}
}  // namespace infra
