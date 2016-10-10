#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <infra/gen/gen-cpp2/configtree_types.h>
#include <infra/ConnectionCache.h>
#include <infra/ModuleProvider.h>
#include <infra/CoordinationClient.h>
#include <infra/gen/gen-cpp2/commontypes_constants.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/gen-ext/commontypes_ext.h>
#include <infra/Serializer.tcc>

namespace infra {

struct ConnectionCache::CacheItem {
    CacheItem() {
        serviceVersion = commontypes_constants::INVALID_VERSION();
    }
    ServiceInfo         serviceInfo;
    int64_t             serviceVersion;
};

ConnectionCache::ConnectionCache(const std::string &logContext,
                                 ModuleProvider* provider)
{
    logContext_ = logContext;
    provider_ = provider;
}

ConnectionCache::~ConnectionCache()
{
    CLog(INFO) << "Exiting ConnectionCache";
}

void ConnectionCache::init()
{
    /* TODO(Rao): Register to receive service updates */
    auto f = [this](int64_t sequenceNo, const std::string &payload) {
        CLog(INFO) << "ConnectionCache update version:" << sequenceNo << " info:" << payload;
#if 0
        ServiceInfo info;
        deserializeFromThriftJson<ServiceInfo>(payload, info, getLogContext());
        CLog(INFO) << "ConnectionCache update version:" << version << " info:" << info;
        // TODO: Update cache in event base
#endif
    };
    provider_->getCoordinationClient()->\
        subscribeToTopic(configtree_constants::TOPIC_SERVICES(), f);
    CLog(INFO) << "ConnectionCache subscribed to topic:" << configtree_constants::TOPIC_SERVICES();
    CLog(INFO) << "Initialized ConnectionCache";
}

std::string ConnectionCache::getConnectionId(const ServiceInfo& info)
{
    return folly::sformat("{}.{}", info.dataSphereId, info.id);
}

void ConnectionCache::fetchMyDatasphereEntries_()
{
    auto servicesRootPath = folly::sformat(configtree_constants::SERVICES_ROOT_PATH_FORMAT(),
                                           provider_->getDatasphereId());
    /* We assume this function is called for service initialization context
     * That is why there is no locking necessary
     */
    CHECK(connections_.size() == 0);

    /* NOTE: This will block.  This is fine as long as we only do it from
     * service initialization context
     */
    auto serviceList = provider_->\
                       getCoordinationClient()->\
                       getChildrenSync(servicesRootPath);
    for (const auto &kvd: serviceList) {
        auto entry = std::make_shared<CacheItem>();
        entry->serviceVersion = getVersion(kvd);
        entry->serviceInfo = deserializeThriftJsonData<ServiceInfo>(kvd, getLogContext());
        connections_[getConnectionId(entry->serviceInfo)] = entry;

        CLog(INFO) << "Added " << entry->serviceInfo << " version:" << entry->serviceVersion;
    }
}

}  // namespace infra
