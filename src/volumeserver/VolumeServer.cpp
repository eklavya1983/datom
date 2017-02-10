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
#include <infra/typestr.h>
#include <infra/gen/gen-cpp2/pbapi_types.tcc>
#include <infra/MessageUtils.tcc>
#include <infra/PBResourceMgr.tcc>

#include <folly/io/async/EventBase.h>
#include <infra/PBMember.h>
#include <infra/PBMember.tcc>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/gen-ext/KVBuffer_ext.tcc>
#include <infra/gen/gen-cpp2/volumeapi_types.h>
#include <infra/gen/gen-cpp2/volumeapi_types.tcc>
#include <volumeserver/VolumeHandleIf.h>
#include <volumeserver/VolumeMetaDb.h>

template <>
const char* typeStr<infra::PullJournalEntriesMsg>() {
    return "PullJournalEntriesMsg";
}

namespace volume {



VolumeReplica::VolumeReplica(const std::string &logCtx,
                             folly::EventBase *eb,
                             ModuleProvider *provider,
                             const std::vector<std::string> &members,
                             const uint32_t &quorum,
                             const VolumeInfo &volumeInfo)
    : PBMember(logCtx,
               eb,
               provider,
               volumeInfo.id,
               folly::sformat(
                   configtree_constants::PB_SPHERE_RESOURCE_ROOT_PATH_FORMAT(),
                   provider->getDatasphereId(),
                   configtree_constants::PB_VOLUMES_TYPE(),
                   volumeInfo.id),
               members,
               provider->getServiceId(),
               quorum),
    db_(logCtx,
        folly::sformat("{}/{}_db",
                       provider->getNodeRoot()->getVolumesPath(),
                       volumeInfo.id))
{
    db_.init();
}

void VolumeReplica::runSyncProtocol()
{
    DCHECK(eb_->isInEventBaseThread());

    auto f = notifyLeader_();
    f
    .then([this](std::unique_ptr<AddToGroupRespMsg> resp) {
        auto promise = std::make_shared<VoidPromise>();
        auto retFut = promise->getFuture();
        CLog(INFO) << "Initiating pull of journal entries from [" << commitId_
            << ", " << resp->syncCommitId << "]";
        pullJournalEntries_(resp->syncCommitId, promise);
        return  retFut;
    })
    .then([this]() {
        return applyBufferedJournalEntries_();
    })
    .then([this]() {
          // TODO(Rao): Complete sync operation
          switchState_(PBMemberState::FOLLOWER_FUNCTIONAL, "Sync completed");
          notifyLeader_();
    })
    .onError([this](const folly::exception_wrapper &ew) {
        CHECK(false) << "Not implemented";
    });
}

folly::Future<std::unique_ptr<AddToGroupRespMsg>>
VolumeReplica::notifyLeader_()
{
    auto msg = AddToGroupMsg();
    msg.resourceId = resourceId_;
    msg.termId = termId_;
    msg.memberId =  myId_;
    msg.memberVersion = version_;
    msg.memberState = state_;

    return sendKVBMessage<AddToGroupMsg, AddToGroupRespMsg>(
        provider_->getConnectionCache(),
        leaderId_,
        msg);
}

void VolumeReplica::pullJournalEntries_(const int64_t &pullEndCommitId,
                                        const std::shared_ptr<VoidPromise> &promise)
{
    DCHECK(eb_->isInEventBaseThread());
    DCHECK(pullEndCommitId >= commitId_);

    if (commitId_ == pullEndCommitId) {
        /* Finshed pull */
        CLog(INFO) << "Finished pulling journal entries";
        promise->setValue();
        return;
    }

    /* Send request to pull journal log entires */
    auto msg = PullJournalEntriesMsg();
    msg.resourceId =  resourceId_;
    msg.termId =  termId_;
    msg.fromId = commitId_;
    msg.toId = pullEndCommitId;
    msg.maxBytesInResp = commontypes_constants::MAX_PAYLOAD_BYTES();
    // TODO(rao): set lease time

    CVLog(LREPLICATION) << toJsonString(msg);
    auto f = sendKVBMessage<PullJournalEntriesMsg, PullJournalEntriesRespMsg>(
        provider_->getConnectionCache(),
        leaderId_,
        msg);
    f
    .then([this, pullEndCommitId, promise](std::unique_ptr<PullJournalEntriesRespMsg> resp) {
        applyJournalEntries_(std::move(resp));
        pullJournalEntries_(pullEndCommitId, promise); 
    })
    .onError([promise](folly::exception_wrapper ew){
        promise->setException(ew);
    });
}

void VolumeReplica::applyJournalEntries_(std::unique_ptr<PullJournalEntriesRespMsg> msg)
{
    DCHECK(eb_->isInEventBaseThread());
    // TODO(Rao): 
}

folly::Future<folly::Unit> VolumeReplica::applyBufferedJournalEntries_()
{
    DCHECK(eb_->isInEventBaseThread());
    // TODO(Rao): 
    return folly::makeFuture();
}

void VolumeReplica::applyUpdate(const KVBuffer &kvb)
{
    DCHECK(!"unimplemented");
}

folly::Future<std::unique_ptr<UpdateBlobRespMsg>> 
VolumeReplica::updateBlob(std::unique_ptr<UpdateBlobMsg> msg)
{
#if 0
    return
        chunkClusterHandle
            ->updateChunks()
            .then([]() {
                  volMetaHandle->updateBlobMeta();
            });
#endif
    return folly::makeFuture(std::make_unique<UpdateBlobRespMsg>());
}

folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
VolumeReplica::updateBlobMeta(std::unique_ptr<UpdateBlobMetaMsg> msg)
{
    return
    via(eb_).then([this, msg = std::move(msg)]() mutable {
        return groupWriteInEb_<UpdateBlobMetaMsg, UpdateBlobMetaRespMsg>(
            [this](std::unique_ptr<UpdateBlobMetaMsg> msg,
                   std::unique_ptr<folly::IOBuf> buffer) {
                return updateBlobMetaLocal(std::move(msg), std::move(buffer));
            },
            std::move(msg));
    });
}

folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
VolumeReplica::updateBlobMetaLocal(std::unique_ptr<UpdateBlobMetaMsg> msg,
                                   std::unique_ptr<folly::IOBuf> buffer)
{
    DCHECK(eb_->isInEventBaseThread());
    return db_.updateBlobMeta(msg, buffer);
}

void VolumeServer::init()
{
    Service::init();
    Service::initIOThreadpool_(2);
    replicaMgr_ = std::make_shared<VolumeReplicaMgr>(this,
                                                     configtree_constants::PB_VOLUMES_TYPE());
    replicaMgr_->init();

    registerHandlers_();
}

void VolumeServer::registerHandlers_()
{
    auto handler = getHandler<ServiceApiHandler>();
    handler->registerKVBMessageHandler(
        typeStr<GetMemberStateMsg>(),
        KVBHandler<GetMemberStateMsg, GetMemberStateRespMsg>(
            [this](std::unique_ptr<GetMemberStateMsg> req) {
                auto replica = replicaMgr_->getResourceOrThrow(req->resourceId);
                return replica->handleGetMemberStateMsg(std::move(req));
            })
        );

    handler->registerKVBMessageHandler(
        typeStr<BecomeLeaderMsg>(),
        KVBOnewayHandler<BecomeLeaderMsg>(
            [this](std::unique_ptr<BecomeLeaderMsg> req) {
                auto replica = replicaMgr_->getResourceOrThrow(req->resourceId);
                replica->handleBecomeLeaderMsg(std::move(req));
            })
        );

    handler->registerKVBMessageHandler(
        typeStr<AddToGroupMsg>(),
        KVBHandler<AddToGroupMsg, AddToGroupRespMsg>(
            [this](std::unique_ptr<AddToGroupMsg> req) {
                auto replica = replicaMgr_->getResourceOrThrow(req->resourceId);
                return replica->handleAddToGroupMsg(std::move(req));
            })
        );

    handler->registerKVBMessageHandler(
        typeStr<GroupInfoUpdateMsg>(),
        KVBOnewayHandler<GroupInfoUpdateMsg>(
            [this](std::unique_ptr<GroupInfoUpdateMsg> req) {
                auto replica = replicaMgr_->getResourceOrThrow(req->resourceId);
                replica->handleGroupInfoUpdateMsg(std::move(req));
            })
        );

    handler->registerKVBMessageHandler(
        typeStr<UpdateBlobMetaMsg>(),
        KVBHandler<UpdateBlobMetaMsg, UpdateBlobMetaRespMsg>(
            [this](std::unique_ptr<UpdateBlobMetaMsg> req) {
                auto replica = replicaMgr_->getResourceOrThrow(req->resourceId);
                return via(replica->getEventBase())
                .then([replica, req=std::move(req)]() mutable {
                    return replica->updateBlobMetaLocal(std::move(req), nullptr);
                });
            })
        );
}

}  // namespace volume
