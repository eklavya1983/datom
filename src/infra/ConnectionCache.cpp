#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
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

folly::Future<std::shared_ptr<at::HeaderClientChannel>>
ConnectionCache::getHeaderClientChannel(const std::string &serviceId)
{
    {
        SharedLock<folly::SharedMutex> l(connectionsMutex_);
        auto itr = connections_.find(serviceId);
        if (itr != connections_.end()) {
            return folly::makeFuture(itr->second.channel);
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
std::shared_ptr<at::HeaderClientChannel>
ConnectionCache::getHeaderClientChannelFromCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    auto itr = connections_.find(serviceId);
    if (itr != connections_.end()) {
        return itr->second.channel;
    } else {
        return nullptr;
    }
}

bool ConnectionCache::existsInCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    return connections_.find(serviceId) != connections_.end();
}

#if 0
/* This class ensure HeaderClientChannel create in updateConnection_
 * is destroyed on eventbase.  This assumes that eventbase is running
 * when HeaderClientChannel is being destroyed.  In typical use case
 * that assumption is true.
 */
/* NOTE: This approach didn't work. I got a segfault I couldn't explain.  I
 * didn't have the time to pursue more
 */
struct ChannelDestroyer {
    void operator()(at::HeaderClientChannel *channel) const {
        auto eb = channel->getEventBase();
        eb->runInEventBaseThread([channel]() { /*channel->destroy();*/ });
    }
};
#endif

std::shared_ptr<at::HeaderClientChannel>
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
        folly::EventBase* eb = nullptr;
        auto socket = ata::TAsyncSocket::newSocket(nullptr);
        auto channel = std::shared_ptr<at::HeaderClientChannel>(
            new at::HeaderClientChannel(socket), folly::DelayedDestruction::Destructor());
        /* Close old channel */
        if (itr != connections_.end()) {
            auto oldChannel = itr->second.channel;
            eb = oldChannel->getEventBase();
            eb->runInEventBaseThread([this, serviceId = itr->first, conn = itr->second]() {
                CLog(INFO) << "Closing old connection against service:" << serviceId
                    << " version:" << conn.version;
                conn.channel->closeNow();
            });
        } else {
            eb = provider_->getEventBaseFromPool();
        }
        /* Init new channel.  AsyncSocket::init() needs to run in eventbase */
        eb->runInEventBaseThread([this, eb, channel, socket, serviceInfo]() {
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
        connections_[serviceInfo.id] = ConnectionItem(version, channel);
        return channel;
    } else {
        CLog(INFO) << "Updated not applied existing version:"
            << ((itr == connections_.end()) ? -1 : itr->second.version)
            << " new version:" << version
            << serviceInfo;
        if (itr == connections_.end()) {
            return nullptr;
        }
        return itr->second.channel;
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
