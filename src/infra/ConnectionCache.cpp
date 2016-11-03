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
#include <infra/LockHelper.tcc>

namespace infra {

const int32_t ConnectionCache::CONNECTION_TIMEOUT_MS = 5000;

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
        auto kvb = deserializeFromThriftJson<KVBinaryData>(payload, getLogContext());
        updateConnection_(kvb, false);
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

folly::Future<std::shared_ptr<apache::thrift::async::TAsyncSocket>>
ConnectionCache::getAsyncSocket(const std::string &serviceId)
{
    {
        SharedLock<folly::SharedMutex> l(connectionsMutex_);
        auto itr = connections_.find(serviceId);
        if (itr != connections_.end()) {
            return folly::makeFuture(itr->second.socket);
        }
    }

    auto serviceEntryKey = folly::sformat(
        configtree_constants::SERVICE_ROOT_PATH_FORMAT(),
        provider_->getDatasphereId(),
        serviceId); 
    CVLog(1) << "cache miss for service:" << serviceId
        << " will try configdb for key: " << serviceEntryKey;
    return provider_->getCoordinationClient()
        ->get(serviceEntryKey)
        .then([this](const KVBinaryData &data) {
              return updateConnection_(data, true);
        });
}
std::shared_ptr<ata::TAsyncSocket>
ConnectionCache::getAsyncSocketFromCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    auto itr = connections_.find(serviceId);
    if (itr != connections_.end()) {
        return itr->second.socket;
    } else {
        return nullptr;
    }
}

bool ConnectionCache::existsInCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    return connections_.find(serviceId) != connections_.end();
}

std::shared_ptr<apache::thrift::async::TAsyncSocket>
ConnectionCache::updateConnection_(const KVBinaryData &kvb, bool createIfMissing)
{
    auto serviceInfo = deserializeThriftJsonData<ServiceInfo>(kvb, getLogContext());
    auto version = getVersion(kvb);

    std::unique_lock<folly::SharedMutex> l(connectionsMutex_);
    auto itr = connections_.find(serviceInfo.id);
    /* Update if new version is higher or we are asked create if entry is
     * missing
     */
    if ((itr == connections_.end() && createIfMissing) ||
        (itr != connections_.end() && version > itr->second.version)) {
        /* Done as below because AsyncSocket::init() needs to run in eventbase */
        auto socket = std::make_shared<ata::TAsyncSocket>(nullptr);
        auto eb = provider_->getEventBaseFromPool();
        eb->runInEventBaseThread([this, eb, socket, serviceInfo]() {
            socket->attachEventBase(eb);
            socket->connect(nullptr,
                            serviceInfo.ip,
                            serviceInfo.port,
                            ConnectionCache::CONNECTION_TIMEOUT_MS);
            CLog(INFO) << "Connection attempted against ip:" << serviceInfo.ip
                << " port:" << serviceInfo.port;
        });
        CLog(INFO) << "Updated connection cache.  New version:" << version
            << serviceInfo;
        connections_[serviceInfo.id] = ConnectionItem(version, socket);
        return socket;
    } else {
        CLog(INFO) << "Updated not applied existing version:"
            << ((itr == connections_.end()) ? -1 : itr->second.version)
            << " new version:" << version
            << serviceInfo;
        if (itr == connections_.end()) {
            return nullptr;
        }
        return itr->second.socket;
    }
}

#if 0
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
#endif

}  // namespace infra
