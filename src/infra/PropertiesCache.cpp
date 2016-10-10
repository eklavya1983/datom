#include <folly/futures/Future.h>
#include <infra/PropertiesCache.h>
#include <infra/Serializer.tcc>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/CoordinationClient.h>


namespace infra {

PropertiesCache::PropertiesCache(const std::string logContext,
                                 const std::string &rootPath,
                                 const std::shared_ptr<CoordinationClient> &coordinationClient)
{
    logContext_ = logContext;
    rootPath_ = rootPath;
    coordinationClient_ = coordinationClient;
}

void PropertiesCache::init()
{
    auto f = coordinationClient_->get("properties/static");
    // NOTE: f.get() can throw an exception
    auto data = f.get();
    /* Deserialize data */
    Properties props = deserializeFromThriftJson<Properties>(data.data, logContext_);
    for (const auto &kv : props.props) {
        cache_[kv.first] = kv.second;
    }
}

}  // namespace infra
