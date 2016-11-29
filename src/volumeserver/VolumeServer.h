#pragma once

#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <infra/Service.h>
#include <folly/SharedMutex.h>

namespace volume {

using namespace infra;

struct VolumeServer;
struct VolumeReplica;

#if 0
template<class ParentT, class ResourceInfoT>
struct PBResourceReplica;
#endif

template <class ParentT, class ResourceT>
struct PBResourceMgr;

using VolumeReplicaMgr = PBResourceMgr<VolumeServer, VolumeReplica>;

struct VolumeServer : Service {
    using Service::Service;

    virtual ~VolumeServer() = default;

    void init() override;

 protected:
    void registerHandlers_();

    std::shared_ptr<VolumeReplicaMgr>                           replicaMgr_; 
};

}  // namespace config
