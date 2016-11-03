#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <folly/SharedMutex.h>

namespace infra {

struct ModuleProvider;

struct ConnectionItem {
    ConnectionItem() = default;
    ConnectionItem(int64_t v,
                   const std::shared_ptr<ata::TAsyncSocket> &s)
        : version(v),
        socket(s)
    {}

    int64_t                             version{0};
    std::shared_ptr<ata::TAsyncSocket>  socket;
};

/**
 * @brief Caches sockets
 */
struct ConnectionCache {
    static const int32_t CONNECTION_TIMEOUT_MS;

    ConnectionCache(const std::string &logContext,
                    ModuleProvider* provider);
    virtual ~ConnectionCache();
    virtual void init();

    std::string getConnectionId(const ServiceInfo& info);

    folly::Future<std::shared_ptr<ata::TAsyncSocket>>
        getAsyncSocket(const std::string &serviceId);

    std::shared_ptr<ata::TAsyncSocket> getAsyncSocketFromCache(const std::string &serviceId);

    bool existsInCache(const std::string &serviceId);

    template <class ClientT>
    folly::Future<std::shared_ptr<ClientT>> getAsyncClient(const std::string &serviceId);

    const std::string& getLogContext() const { return logContext_; }

 protected:
    ConnectionCache(const ConnectionCache&) = delete;
    ConnectionCache operator=(const ConnectionCache&) = delete;

    std::shared_ptr<ata::TAsyncSocket>
        updateConnection_(const KVBinaryData &kvb, bool createIfMissing);
#if 0
    void fetchMyDatasphereEntries_();
#endif

    std::string                                                     logContext_;
    ModuleProvider                                                  *provider_;
    folly::SharedMutex                                              connectionsMutex_;
    std::unordered_map<std::string, ConnectionItem>                 connections_;
};

}
