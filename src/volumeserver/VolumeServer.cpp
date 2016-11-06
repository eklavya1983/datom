#include <volumeserver/VolumeServer.h>
#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <util/Log.h>
#include <infra/ZooKafkaClient.h>
#include <infra/Service.h>
#include <infra/Serializer.tcc>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/gen-ext/commontypes_ext.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>
#include <infra/ConnectionCache.h>
#include <infra/MessageException.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <infra/LockHelper.tcc>

#include <folly/io/async/EventBase.h>
#include <infra/PBMember.h>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>

namespace volumeserver {

struct VolumeReplica : PBMember {
    using ResourceInfoType = VolumeInfo;

    VolumeReplica(const std::string &logCtx,
                  folly::EventBase *eb,
                  ModuleProvider *provider,
                  const std::vector<std::string> &members,
                  const uint32_t &quorum,
                  const VolumeInfo &volumeInfo)
        : PBMember(logCtx,
                   eb,
                   provider,
                   folly::sformat(
                       configtree_constants::PB_SPHERE_RESOURCE_ROOT_PATH_FORMAT(),
                       provider->getDatasphereId(), configtree_constants::PB_VOLUMES_TYPE()),
                   members,
                   provider->getServiceId(),
                   quorum)
    {
    }

    virtual void applyUpdate(const KVBinaryData &kvb)
    {
    }
};
#if 0
template<class ParentT, class ResourceInfoT>
struct PBResourceReplica {
    using ResourceInfoType = ResourceInfoT;

    PBResourceReplica(const std::string &logCtx,
                      folly::EventBase *eb,
                      ParentT *parent,
                      const int64_t &version,
                      const std::string &groupKey,
                      const std::vector<std::string> &members,
                      const uint32_t &quorum,
                      const ResourceInfoT& info)
        : parent_(parent)
    {
    }
    virtual void init()
    {
    }
    virtual void applyUpdate(const KVBinaryData &kvb)
    {
        DCHECK(!"Unimplemented");
    }
 protected:
    ParentT                 *parent_;
};
#endif

// TODO(Rao):
// 1. Subscribe to events for volume table
// 2. Move to PBResourceMgr file
template <class ParentT, class ResourceT>
struct PBResourceMgr {
    using ResourceTable = std::unordered_map<int64_t, std::shared_ptr<ResourceT>>;

    PBResourceMgr(ParentT* parent, const std::string &id)
    {
        parent_ = parent;
        id_ = id;
        logContext_ = folly::sformat("{}:PBResourceMgr:{}", parent->getLogContext(), id_);
    }
    virtual ~PBResourceMgr() = default;
    virtual void init()
    {
        pullResourceConfigTable_();
    }
    virtual void addResource()
    {
        CHECK(!"Unimplemented");
    }
    virtual std::shared_ptr<ResourceT> getResource()
    {
        CHECK(!"Unimplemented");
        return nullptr;
    }
    const std::string& getLogContext() const {
        return logContext_;
    }

 protected:
    virtual void pullResourceConfigTable_()
    {
        /* Pull ring table */
        CLog(INFO) << "Pulling rings from configdb";
        auto ringsRoot = folly::sformat(configtree_constants::PB_SPHERE_RINGS_ROOT_PATH_FORMAT(),
                                        parent_->getDatasphereId(),
                                        id_);
        auto ringVector = parent_->getCoordinationClient()->getChildrenSync(ringsRoot);
        for (const auto &kvb : ringVector) {
            auto id = folly::to<int64_t>(getId(kvb));
            ringTable_[id] = deserializeThriftJsonData<RingInfo>(kvb, getLogContext());
        }

        /* Pull resource table from configdb */
        CLog(INFO) << "Pulling resources from configdb";
        std::vector<KVBinaryData> resourceVector;
        auto resourcesRoot = folly::sformat(
            configtree_constants::PB_SPHERE_RESOURCES_ROOT_PATH_FORMAT(),
            parent_->getDatasphereId(),
            id_);
        try {
            resourceVector = parent_->getCoordinationClient()->getChildrenSync(resourcesRoot);
        } catch (const StatusException &e) {
            if (e.getStatus() != Status::STATUS_INVALID_KEY) {
                throw;
            }
        }

        {
            std::unique_lock<folly::SharedMutex> l(resourceTableMutex_);
            for (const auto &kvb : resourceVector) {
                addResourceReplicaIfOwned_(kvb);
            }
        }

        /* register for updates */
        auto resourcesTopic = folly::sformat(
            configtree_constants::TOPIC_PB_SPHERE_RESOURCES(),
            id_);
        parent_->getCoordinationClient()->\
        subscribeToTopic(resourcesTopic,
                         [this](int64_t sequenceNo, const std::string& payload) {
                            CLog(INFO) << "Received message: " << sequenceNo;
#if 0
                            auto kvb = deserializeFromThriftJson<KVBinaryData>(payload,
                                                                               getLogContext());
                             handleResourceConfigTableUpdate_(kvb);
#endif
                         });
    }

    virtual void addResourceReplicaIfOwned_(const KVBinaryData &kvb)
    {
        auto id = folly::to<int64_t>(getId(kvb));
        auto version = getVersion(kvb);
        auto resourceInfo = deserializeThriftJsonData<typename ResourceT::ResourceInfoType>(kvb, getLogContext());
        if (isRingMember(ringTable_[resourceInfo.ringId], parent_->getServiceId())) {
            auto resource = std::make_shared<ResourceT>(logContext_, 
                                                        parent_->getEventBaseFromPool(),
                                                        parent_,
                                                        ringTable_[resourceInfo.ringId].memberIds,
                                                        2,
                                                        resourceInfo);
            resourceTable_[id] = resource;
            resource->init();
            CLog(INFO) << "Added resource:" << id << " version:" << version
                << toJsonString(resourceInfo);
        } else {
            CVLog(1) << "Ignoring unowned resource update id:" << id << " version:" << version;
        }
    }

    virtual void handleResourceConfigTableUpdate_(const KVBinaryData &kvb)
    {
        auto id = folly::to<int64_t>(getId(kvb));

        std::unique_lock<folly::SharedMutex> l(resourceTableMutex_);
        auto itr = resourceTable_.find(id);
        if (itr == resourceTable_.end()) {
            addResourceReplicaIfOwned_(kvb);
        } else {
            itr->second->applyUpdate(kvb);
        }
    }

    ParentT                                             *parent_;
    std::string                                         logContext_;
    std::string                                         id_;
    folly::SharedMutex                                  resourceTableMutex_;
    std::unordered_map<uint64_t, RingInfo>              ringTable_;
    ResourceTable                                       resourceTable_;
};

void VolumeServer::init()
{
    Service::init();
    Service::initIOThreadpool_(2);
    replicaMgr_ = std::make_shared<VolumeReplicaMgr>(this,
                                                     configtree_constants::PB_VOLUMES_TYPE());
    replicaMgr_->init();
}

}  // namespace volumeserver 
