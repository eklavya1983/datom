#pragma once

#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <infra/Service.h>
#include <infra/PBMember.h>
#include <infra/Journal.h>
#include <volumeserver/VolumeHandleIf.h>
#include <folly/SharedMutex.h>
#include <volumeserver/VolumeMetaDb.h>

namespace infra {
template <class ParentT, class ResourceT>
struct PBResourceMgr;
struct Journal;
}

namespace volume {

using namespace infra;

struct VolumeServer;
class UpdateBlobRespMsg;
class UpdateBlobMsg;
class UpdateBlobMetaRespMsg;
class UpdateBlobMetaMsg;

struct SyncCtx {
    Journal             activeIoJournal_;
};

struct VolumeReplica : PBMember, VolumeHandleIf {
    using ResourceInfoType = VolumeInfo;

    VolumeReplica(const std::string &logCtx,
                  folly::EventBase *eb,
                  ModuleProvider *provider,
                  const std::vector<std::string> &members,
                  const uint32_t &quorum,
                  const VolumeInfo &volumeInfo);

    /* Overrides from PBMember */

    void applyResourceUpdate(const KVBuffer &kvb);

    /* Leader IO methods */
    folly::Future<std::unique_ptr<UpdateBlobRespMsg>>
        updateBlob(std::unique_ptr<UpdateBlobMsg> msg) override;

    folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
        updateBlobMeta(std::unique_ptr<UpdateBlobMetaMsg> msg) override;

    /* Member IO methods */
    template<class ReqT, class RespT>
    folly::Future<std::unique_ptr<RespT>> handleMetadataWrite(std::unique_ptr<ReqT> msg,
                                                              std::unique_ptr<folly::IOBuf> buffer);
    
    /* Sync related messages */
    void runSyncProtocol() override;
    folly::Future<std::unique_ptr<PullJournalEntriesRespMsg>>
        handlePullJournalEntriesMsg(std::unique_ptr<PullJournalEntriesMsg> msg);
    folly::Future<std::vector<std::unique_ptr<KVBuffer>>>
        applyOpBatch(std::vector<JournalEntry> &entries);

 protected:
    folly::Future<std::unique_ptr<AddToGroupRespMsg>> notifyLeader_();
    void pullJournalEntries_(const int64_t &pullEndCommitId,
                             const std::shared_ptr<VoidPromise> &promise);
    folly::Future<folly::Unit> applyBufferedJournalEntries_();

    VolumeMetaDb                db_;
    Journal                     journal_;
    std::unique_ptr<SyncCtx>    syncCtx_;
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
