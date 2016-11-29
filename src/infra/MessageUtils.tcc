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
    setAsBinaryPayload<ReqT>(*reqKvb, req);

    auto f = 
        connMgr
        ->getAsyncClient<ServiceApiAsyncClient>(id)
        .then([reqKvb=std::move(reqKvb)](const std::shared_ptr<ServiceApiAsyncClient>& client) {
            return client 
                    ->future_handleKVBMessage(*reqKvb)
                    .then([](const KVBinaryData &respKvb) {
                          return getFromBinaryPayload<RespT>(respKvb);
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
    setAsBinaryPayload<ReqT>(*reqKvb, req);
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

template <class ReqT, class RespT>
struct KVBHandler{
    using Handler = std::function<folly::Future<std::unique_ptr<RespT>> (std::unique_ptr<ReqT>)>;
    KVBHandler(const Handler &handler)
        : handler_(handler)
    {
    }
    folly::Future<std::unique_ptr<KVBinaryData>> operator()(std::unique_ptr<KVBinaryData> kvb)
    {
        std::unique_ptr<ReqT> req(new ReqT);
        *req = getFromBinaryPayload<ReqT>(*kvb);
        return
            handler_(std::move(req))
            .then([](std::unique_ptr<RespT> resp) {
                std::unique_ptr<KVBinaryData> retKvb (new KVBinaryData);
                setAsBinaryPayload<RespT>(*retKvb, *resp);
                return retKvb;
            });
    }

 private:
    Handler handler_;
};

template <class ReqT>
struct KVBOnewayHandler{
    using Handler = std::function<void (std::unique_ptr<ReqT>)>;
    KVBOnewayHandler(const Handler &handler)
        : handler_(handler)
    {
    }
    folly::Future<std::unique_ptr<KVBinaryData>> operator()(std::unique_ptr<KVBinaryData> kvb)
    {
        std::unique_ptr<ReqT> req(new ReqT);
        *req = getFromBinaryPayload<ReqT>(*kvb);
        handler_(std::move(req));
        return folly::makeFuture(std::make_unique<KVBinaryData>());
    }

 private:
    Handler handler_;
};


}  // namespace infra
