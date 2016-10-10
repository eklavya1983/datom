#pragma once

#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <infra/Service.h>
#include <folly/SharedMutex.h>

namespace volumeserver {

using namespace infra;

struct VolumeServer;

template<class ParentT, class ResourceInfoT>
struct PBResourceReplica;

template <class ParentT, class ResourceT>
struct PBResourceMgr;

using VolumeReplicaMgr = PBResourceMgr<VolumeServer, PBResourceReplica<VolumeServer, VolumeInfo>>;

struct VolumeServer : Service {
    using Service::Service;

    virtual ~VolumeServer() = default;

    void init() override;
    folly::EventBase* getEventBaseFromPool() override;

 protected:
    std::shared_ptr<wangle::IOThreadPoolExecutor>               ioThreadpool_;
    std::shared_ptr<VolumeReplicaMgr>                           replicaMgr_; 
};

}  // namespace config
