#pragma once

#include <memory>
#include <KVStore.h>
#include <unordered_map>

extern "C" {
    struct zhandle_t;
}

namespace cluster {

struct ZookeeperAdapter;
using ZookeeperAdapterSPtr = std::shared_ptr<ZookeeperAdapter>;


/**
 * @brief Zookeeper based key value store
 */
struct ZookeeperClient {
    ZookeeperKVStore(const std::string &logContext,
                     const std::string &zkServers); 
    void init();

    std::string getCachedValue(const std::string &key) override;
    folly::Future<std::string> getCurrentValue(const std::string &key) override;
    folly::Future<folly::Unit> put(const std::string &key, const std::string &value) override;
    void watch(const std::string &key, const WatchCb &cb) override;

    inline std::string getLogContext() const { return logContext_; }

 private:
    void refreshCache_();

    std::string                                     logContext_;
    std::string                                     zkServers_;
    zhandle_t*                                      zkHandle_ {nullptr};
    std::unordered_map<std::string, std::string>    cache_;
};

}  // namespace
