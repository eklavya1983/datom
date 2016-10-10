#pragma once

#include <infra/PropertiesCache.h>
#include <folly/Conv.h>

namespace infra {

template<class T>
T PropertiesCache::getProp(const std::string &key)
{
    cache_.at(key);
}

template<class T>
T PropertiesCache::getProp(const std::string &key, const T &fallback)
{
    try {
        return getProp<T>(key);
    } catch (const std::exception &e) {
        return fallback;
    }
}

}  // namespace infra
