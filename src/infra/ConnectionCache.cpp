#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen-ext/KVBuffer_ext.tcc>
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

ConnectionItem::ConnectionItem(ConnectionCache *parent,
                               folly::EventBase *eb)
    : parent_(parent),
    eb_(eb),
    version_(-1)
{
}

const std::string& ConnectionItem::getLogContext() const
{
    return parent_->getLogContext();
}

folly::Future<std::shared_ptr<at::HeaderClientChannel>>
ConnectionItem::update(int64_t version, const ServiceInfo &info)
{
    return via(eb_).then([this, version, info]() {
        if (version <= version_) {
            CLog(INFO) << "Update not applied existing version:" << version_
                << " new version:" << version
                << info;
            return channel_;
        }
        /* Close old channel */
        if (channel_) {
            CVLog(2) << "Closing old connection against id:" << info.id;
            channel_->closeNow();
        }
        auto socket = ata::TAsyncSocket::newSocket(eb_,
                                                   info.ip,
                                                   info.port,
                                                   ConnectionCache::CONNECTION_TIMEOUT_MS);
        channel_ = std::shared_ptr<at::HeaderClientChannel>(
            new at::HeaderClientChannel(socket),
            folly::DelayedDestruction::Destructor());
        return channel_;
    });
}

folly::Future<std::shared_ptr<at::HeaderClientChannel>>
ConnectionItem::getChannel()
{
    return via(eb_).then([this]() {
        return channel_;
    });
}

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
    /* Register to receive service updates */
    auto f = [this](int64_t sequenceNo, const std::string &payload) {
        auto kvb = deserializeFromThriftJson<KVBuffer>(payload, getLogContext());
        CVLog(2) << "Connection update:" << toJsonString(kvb);
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
            return itr->second.getChannel();
        }
    }

    auto serviceEntryKey = folly::sformat(
        configtree_constants::SERVICE_ROOT_PATH_FORMAT(),
        provider_->getDatasphereId(),
        serviceId); 
    CVLog(2) << "cache miss for service:" << serviceId
        << " will try configdb for key: " << serviceEntryKey;
    return provider_->getCoordinationClient()
        ->get(serviceEntryKey)
        .then([this](const KVBuffer &data) {
              CVLog(2) << "Fetched service entry:" << toJsonString(data);
              return updateConnection_(data, true);
        });
}

folly::Future<std::shared_ptr<at::HeaderClientChannel>>
ConnectionCache::getHeaderClientChannelFromCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    auto itr = connections_.find(serviceId);
    if (itr != connections_.end()) {
        return itr->second.getChannel();
    } else {
        return makeFuture(std::shared_ptr<at::HeaderClientChannel>());
    }
}

bool ConnectionCache::existsInCache(const std::string &serviceId)
{
    SharedLock<folly::SharedMutex> l(connectionsMutex_);
    return connections_.find(serviceId) != connections_.end();
}

folly::Future<std::shared_ptr<at::HeaderClientChannel>>
ConnectionCache::updateConnection_(const KVBuffer &kvb, bool createIfMissing)
{
    auto serviceInfo = getFromThriftJsonPayload<ServiceInfo>(kvb);
    auto version = getVersion(kvb);

    std::unique_lock<folly::SharedMutex> l(connectionsMutex_);
    auto itr = connections_.find(serviceInfo.id);
    if (itr == connections_.end()) {
        if (!createIfMissing) {
            /* nothing to update just return */
            return makeFuture(std::shared_ptr<at::HeaderClientChannel>());
        }
        std::tie(itr, std::ignore) = connections_.insert(std::make_pair(serviceInfo.id,
                                                                        ConnectionItem(this, provider_->getEventBaseFromPool())));
    }
    return itr->second.update(version, serviceInfo);
}

}  // namespace infra
