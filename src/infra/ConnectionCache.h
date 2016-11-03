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
                   const std::shared_ptr<at::HeaderClientChannel> &c)
        : version(v),
        channel(c)
    {}

    int64_t                                     version{0};
    /* We cache channel instead of socket because clients are constructed out of
     * channels and when client goes out of scope, if channel refcount is zero,
     * underneath transport is closed.  Since AsyncSocket and channel that is
     * built on top of it is meant to be shared, we need to cache the channel
     */
    std::shared_ptr<at::HeaderClientChannel>    channel;
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

    folly::Future<std::shared_ptr<at::HeaderClientChannel>>
        getHeaderClientChannel(const std::string &serviceId);
    std::shared_ptr<at::HeaderClientChannel> getHeaderClientChannelFromCache(const std::string &serviceId);

    bool existsInCache(const std::string &serviceId);

    template <class ClientT>
    folly::Future<std::shared_ptr<ClientT>> getAsyncClient(const std::string &serviceId);

    const std::string& getLogContext() const { return logContext_; }

 protected:
    ConnectionCache(const ConnectionCache&) = delete;
    ConnectionCache operator=(const ConnectionCache&) = delete;

    std::shared_ptr<at::HeaderClientChannel>
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
