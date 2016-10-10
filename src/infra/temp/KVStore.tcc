#include <KVStore.h>
#include <folly/futures/Future.h>
#include <folly/Conv.h>

namespace cluster {

template<class T>
T KVStore::getCached(const std::string &key)
{
    return folly::to<T>(getCachedValue(key)); 
}

template<class T>
T KVStore::getCached(const std::string &key, const T& fallbackRet)
{
    try {
        return getCached<T>(key);
    } catch (const std::exception &e) {
        return fallbackRet;
    }
}

template<class T>
folly::Future<T> KVStore::getCurrent(const std::string &key)
{
    return getCurrentValue(key).then([](std::string& value) { return folly::to<T>(value); });
}

}  // namespace cluster
