#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace infra {

struct CoordinationClient;

struct PropertiesCache {
    PropertiesCache(const std::string logContext,
                    const std::string &rootPath,
                    const std::shared_ptr<CoordinationClient> &coordinationClient);

    void init();

    template<class T>
    T getProp(const std::string &key);

    template<class T>
    T getProp(const std::string &key, const T &fallback);
    
 protected:
    std::string                                     logContext_;
    std::string                                     rootPath_;
    std::shared_ptr<CoordinationClient>             coordinationClient_;
    std::unordered_map<std::string, std::string>    cache_;
};

};
