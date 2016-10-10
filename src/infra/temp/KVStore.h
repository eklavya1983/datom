#pragma once

#include <string>
#include <functional>

namespace folly {
template <class T>
class Future;
}

namespace cluster {

/**
 * @brief Key value store
 */
struct KVStore {
    using WatchCb = std::function<void (const std::string &key,
                                        const std::string &value)>;
    template<class T>
    T getCached(const std::string &key);

    template<class T>
    T getCached(const std::string &key, const T& fallbackRet);

    template<class T>
    folly::Future<T> getCurrent(const std::string &key);

    virtual std::string getCachedValue(const std::string &key) = 0;
    virtual folly::Future<std::string> getCurrentValue(const std::string &key) = 0;

    virtual folly::Future<folly::Unit> put(const std::string &key, const std::string &value) = 0;

    virtual void watch(const std::string &key, const WatchCb &cb) = 0;
};

}  // namespace cluster
