#pragma once

#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <infra/Service.h>
#include <infra/PBMember.h>
#include <volumeserver/VolumeHandleIf.h>
#include <folly/SharedMutex.h>
#include <volumeserver/VolumeMetaDb.h>

namespace infra {
template <class ParentT, class ResourceT>
struct PBResourceMgr;
}

namespace volume {

using namespace infra;

struct VolumeServer;
class UpdateBlobRespMsg;
class UpdateBlobMsg;
class UpdateBlobMetaRespMsg;
class UpdateBlobMetaMsg;

struct VolumeReplica : PBMember, VolumeHandleIf {
    using ResourceInfoType = VolumeInfo;

    VolumeReplica(const std::string &logCtx,
                  folly::EventBase *eb,
                  ModuleProvider *provider,
                  const std::vector<std::string> &members,
                  const uint32_t &quorum,
                  const VolumeInfo &volumeInfo);

    /* Overrides from PBMember */
    void runSyncProtocol() override;

    void applyUpdate(const KVBuffer &kvb);

    folly::Future<std::unique_ptr<UpdateBlobRespMsg>>
        updateBlob(std::unique_ptr<UpdateBlobMsg> msg) override;

    folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
        updateBlobMeta(std::unique_ptr<UpdateBlobMetaMsg> msg) override;

    folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
        updateBlobMetaLocal(std::unique_ptr<UpdateBlobMetaMsg> msg,
                            std::unique_ptr<folly::IOBuf> buffer);

 protected:
    folly::Future<std::unique_ptr<AddToGroupRespMsg>> notifyLeader_();
    folly::Future<folly::Unit> pullJournalEntries_(const std::shared_ptr<VoidPromise> &promise);
    void applyJournalEntries_(std::unique_ptr<PullJournalEntriesRespMsg> msg);
    folly::Future<folly::Unit> applyBufferedJournalEntries_();

    VolumeMetaDb                db_;
};

#if 0
template<class ParentT, class ResourceInfoT>
struct PBResourceReplica;
#endif

using VolumeReplicaMgr = PBResourceMgr<VolumeServer, VolumeReplica>;

struct VolumeServer : Service {
    using Service::Service;

    virtual ~VolumeServer() = default;

    void init() override;

    std::shared_ptr<VolumeReplicaMgr> getReplicaMgr() const { return replicaMgr_; }

 protected:
    void registerHandlers_();

    std::shared_ptr<VolumeReplicaMgr>                           replicaMgr_; 
};

}  // namespace config
