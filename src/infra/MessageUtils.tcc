#pragma once
#include <string>
#include <memory>
#include <folly/futures/Future.h>
#include <infra/ConnectionCache.h>
#include <infra/gen-ext/KVBuffer_ext.tcc>
#include <infra/gen/gen-cpp2/ServiceApi.h>
#include <infra/typestr.h>
#include <infra/MessageUtils.h>

namespace infra {

/**
 * @brief Send binary message req to service identified id
 * This version returns future that holds the response
 *
 */
template <class ReqT, class RespT>
folly::Future<std::unique_ptr<RespT>> sendKVBMessage(ConnectionCache *connMgr,
                                                     const std::string &id,
                                                     const ReqT& req)
{
    auto reqKvb = std::make_unique<KVBuffer>();
    setType(*reqKvb, typeStr<ReqT>());
    setAsBinaryPayload<ReqT>(*reqKvb, req);

    auto f = 
        connMgr
        ->getAsyncClient<ServiceApiAsyncClient>(id)
        .then([reqKvb=std::move(reqKvb)](const std::shared_ptr<ServiceApiAsyncClient>& client) {
            return client 
                    ->future_handleKVBMessage(*reqKvb)
                    .then([](const KVBuffer &respKvb) {
                          return getFromBinaryPayload<RespT>(respKvb);
                    });
        });
    return f;
}

/**
 * @brief Send binary message req to service identified id
 * This version returns future that doesn't hold the response.  Use this version
 * If you don't care for response.
 */
template <class ReqT>
folly::Future<folly::Unit> sendKVBMessage(ConnectionCache *connMgr,
                                          const std::string &id,
                                          const ReqT& req)
{
    auto reqKvb = std::make_unique<KVBuffer>();
    setType(*reqKvb, typeStr<ReqT>());
    setAsBinaryPayload<ReqT>(*reqKvb, req);
    auto f = 
        connMgr
        ->getAsyncClient<ServiceApiAsyncClient>(id)
        .then([reqKvb=std::move(reqKvb)](const std::shared_ptr<ServiceApiAsyncClient>& client) {
            return client 
                    ->future_handleKVBMessage(*reqKvb)
                    .then([](const KVBuffer &) {
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
    folly::Future<std::unique_ptr<KVBuffer>> operator()(std::unique_ptr<KVBuffer> kvb)
    {
        std::unique_ptr<ReqT> req = getFromBinaryPayload<ReqT>(*kvb);
        threadLocalBuffer = std::move(kvb->payload);
        return
            handler_(std::move(req))
            .then([](std::unique_ptr<RespT> resp) {
                std::unique_ptr<KVBuffer> retKvb (new KVBuffer);
                setAsBinaryPayload<RespT>(*retKvb, *resp);
                setType(*retKvb, typeStr<RespT>());
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
    folly::Future<std::unique_ptr<KVBuffer>> operator()(std::unique_ptr<KVBuffer> kvb)
    {
        std::unique_ptr<ReqT> req = getFromBinaryPayload<ReqT>(*kvb);
        handler_(std::move(req));
        auto retKvb = std::make_unique<KVBuffer>();
        setType(*retKvb, "Unit");
        return folly::makeFuture(std::move(retKvb));
    }

 private:
    Handler handler_;
};


}  // namespace infra
