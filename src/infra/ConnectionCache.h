#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace infra {

struct ServiceInfo;
struct ModuleProvider;

/**
 * @brief Caches sockets
 */
struct ConnectionCache {
    ConnectionCache(const std::string &logContext,
                    ModuleProvider* provider);
    virtual ~ConnectionCache();
    virtual void init();

    std::string getConnectionId(const ServiceInfo& info);
    const std::string& getLogContext() const { return logContext_; }

 protected:
    ConnectionCache(const ConnectionCache&) = delete;
    ConnectionCache operator=(const ConnectionCache&) = delete;

    void fetchMyDatasphereEntries_();

    std::string                                                     logContext_;
    ModuleProvider                                                  *provider_;
    struct CacheItem;
    std::unordered_map<std::string, std::shared_ptr<CacheItem>>     connections_;
};

}
