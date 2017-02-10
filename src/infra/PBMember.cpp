#include <folly/io/async/EventBase.h>
#include <folly/futures/Future.h>
#include <folly/futures/Future-inl.h>
#include <folly/String.h>
#include <folly/Format.h>

#include <infra/ModuleProvider.h>
#include <infra/PBMember.h>
#include <infra/CoordinationClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen/gen-cpp2/pbapi_constants.h>
#include <infra/gen/gen-cpp2/pbapi_types.h>
#include <infra/gen/gen-cpp2/pbapi_types.tcc>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/StatusException.h>
#include <infra/typestr.h>
#include <infra/MessageUtils.tcc>
#include <infra/ConnectionCache.tcc>

#define THROW_IFNOT_LEADER() \
    if (!isLeaderState() || !leaderCtx_) { \
        CLog(WARNING) << "Member isn't a leader"; \
        throw StatusException(Status::STATUS_NOT_LEADER); \
    }


template <>
const char* typeStr<infra::GetMemberStateMsg>() {
    return "GetMemberStateMsg";
}
template <>
const char* typeStr<infra::GetMemberStateRespMsg>() {
    return "GetMemberStateRespMsg";
}
template <>
const char* typeStr<infra::BecomeLeaderMsg>() {
    return "BecomeLeaderMsg";
}
template <>
const char* typeStr<infra::AddToGroupMsg>() {
    return "AddToGroupMsg";
}
template <>
const char* typeStr<infra::AddToGroupRespMsg>() {
    return "AddToGroupRespMsg";
}
template <>
const char* typeStr<infra::GroupInfoUpdateMsg>() {
    return "GroupInfoUpdateMsg";
}

namespace infra {

const int32_t PBMember::GROUPWATCH_INTERVAL_MS = 10000;

PBMember::LeaderCtx::LeaderCtx()
{
    opId = commontypes_constants::INVALID_VALUE();
    commitId = commontypes_constants::INVALID_VALUE();
}

PBMember::PBMember(const std::string &logCtx,
                   folly::EventBase *eb,
                   ModuleProvider *provider,
                   int64_t resourceId,
                   const std::string &groupKey,
                   const std::vector<std::string> &members,
                   const std::string &myId,
                   const uint32_t &quorum)
    : logContext_(logCtx),
    eb_(eb),
    provider_(provider),
    resourceId_(resourceId),
    groupKey_(groupKey),
    memberIds_(members),
    myId_(myId),
    quorum_(quorum)
{
    DCHECK(quorum_ > 0);
    termId_ = commontypes_constants::INVALID_VALUE();
    commitId_ = commontypes_constants::INVALID_VALUE();
}

void PBMember::init()
{
    switchState_(PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP, __FUNCTION__);

    /* Register watch for group changes */
    auto watchF = [this](const std::string &key) {
        watchCb_(key);
    };
    /* Though we don't care for getting children here, this is how we register
     * watch on the group
     */
    provider_->getCoordinationClient()->getChildrenSimple(groupKey_, watchF);

    /* Create sequential ephemeral node for this member to trigger
     * GrouWatchEvent.  First one to join group is the elector.
     * This is how we indicate we are up for rest of the
     * members.
     */
    auto memberRoot = folly::sformat("{}/{}:", groupKey_, myId_);
    auto f = provider_->getCoordinationClient()->createEphemeral(
        memberRoot,
        myId_,
        true);
    f
    .via(eb_)
    .then([this, memberRoot](const std::string &res) {
        CLog(INFO) << "Created member root at:" << memberRoot;
    })
    .onError([this](folly::exception_wrapper ew) {
        handleError_(Status::STATUS_INVALID, "Error creating member root");
    });
}

void PBMember::handleGroupWatchEvent(const std::vector<std::string> &children)
{
    DCHECK(eb_->isInEventBaseThread());

    CVLog(LCONFIG) << "GroupWatchEvent " << toJsonString(children);

    switch (state_) {
        case PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP:
            if (!hasLock_(children) &&
                canIBeElector_(children))  {
                switchState_(PBMemberState::EC_ACQUIRE_LOCK, "handleGroupWatchEvent");
                auto f = acquireLock_(pbapi_constants::PB_LOCKTYPE_ELECTOR());
                f
                .via(eb_)
                .then([this]() {
                    DCHECK(state_ == PBMemberState::EC_ACQUIRE_LOCK);
                    return increaseTerm_();
                })
                .then([this](int64_t term) {
                    DCHECK(state_ == PBMemberState::EC_ACQUIRE_LOCK);
                    termId_ = term;
                    CLog(INFO) << "Term set to:" << termId_;
                    return provider_->getCoordinationClient()->getChildrenSimple(groupKey_);
                })
                .then([this](const std::vector<std::string> &children) {
                    DCHECK(state_ == PBMemberState::EC_ACQUIRE_LOCK);
                    DCHECK(hasLock_(children));
                    switchState_(PBMemberState::EC_WAITING_FOR_QUORUM, "lock acquired");
                    if (hasQuorumMemberCount_(children)) {
                        switchState_(PBMemberState::EC_ELECTION_IN_PROGRESS, "quorum met");
                        issueElectionRequest_();
                    }
                })
                .onError([this](const StatusException &e) {
                    handleError_(e.getStatus(), "transitioning to Elector");
                })
                .onError([this](folly::exception_wrapper ew) {
                    handleError_(Status::STATUS_INVALID, "transitioning to Elector");
                });
            }
            break;
        case PBMemberState::EC_WAITING_FOR_QUORUM:
            DCHECK(hasLock_(children));
            if (hasQuorumMemberCount_(children)) {
                switchState_(PBMemberState::EC_ELECTION_IN_PROGRESS, "quorum met");
                issueElectionRequest_();
            }
            break;
        default:
            break;
    }
}

folly::Future<std::unique_ptr<GetMemberStateRespMsg>>
PBMember::handleGetMemberStateMsg(std::unique_ptr<GetMemberStateMsg> req)
{
    return via(eb_).then([this, req=std::move(req)]() {
        throwIfInvalidTerm_(req->termId);

        auto resp = std::make_unique<GetMemberStateRespMsg>();
        resp->id = myId_;
        resp->commitId = commitId_;
        // TODO(Rao): Set version if required
        resp->version = 0;
        resp->state = state_;

        return resp;
    });
}

void PBMember::handleBecomeLeaderMsg(std::unique_ptr<BecomeLeaderMsg> req)
{
    via(eb_).then([this, req=std::move(req)]() {
        throwIfInvalidTerm_(req->termId);
        // TODO(Rao): Any further necessary checks such as whethere we are in
        // right state to become leader etc.
        auto f = acquireLock_(pbapi_constants::PB_LOCKTYPE_LEADER());
        f
        .via(eb_)
        .then([this, functionalMembers=req->functionalMembers]() {
            DCHECK(functionalMembers[0].id == myId_ &&
                   functionalMembers[0].commitId == commitId_);
            /* Create leader context */
            leaderCtx_ = std::make_unique<LeaderCtx>();
            leaderId_ = myId_;

            /* Partition leaderCtx_->peers into 
             * functional members and nonfunctional members 
             */
            for (const auto &member : memberIds_) {
                if (member == myId_) {
                    continue;
                }
                LeaderCtx::PeerInfo peer;
                peer.id = member;
                auto itr = std::find_if(functionalMembers.begin(),
                                        functionalMembers.end(),
                                        [member](const GetMemberStateRespMsg &resp) {
                                            return member == resp.id;
                                        });
                if (itr != functionalMembers.end()) {
                    peer.state = itr->state;
                    peer.version = itr->version;
                } else {
                    peer.state = PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP;
                    peer.version = commontypes_constants::INVALID_VERSION();
                }
                leaderCtx_->peers.push_back(peer);
            }

            if (functionalMembers.size() >= quorum_) {
                switchState_(PBMemberState::LEADER_FUNCTIONAL,
                             "Become leader message");
            } else {
                switchState_(PBMemberState::LEADER_WAITING_FOR_QUORUM,
                             "Become leader message");
            }
            //TODO(Rao): 
            // 1. Set opid
            // 2. Set leader state based on # of members
            // 3. Send a message to tell the group members we have a funciton group
        })
        .onError([this](folly::exception_wrapper ew) {
            handleError_(Status::STATUS_INVALID, "acquire leader lock");
        });
    });
}

void
PBMember::handleElectionResponse(const std::map<int64_t,
                                 std::vector<GetMemberStateRespMsg>> &values)
{
    DCHECK(eb_->isInEventBaseThread());
    DCHECK(state_ == PBMemberState::EC_ELECTION_IN_PROGRESS);

    /* Determine leader */
    /* For now leader is the entity with latest state.  TODO(Rao): This needs to be
     * imporved so that we take term into account as well
     */
    if (values.size() > 0 && values.begin()->second.size() >= quorum_) {
        lastElectionTimepoint_ = getTimePoint();

        removeLock_(pbapi_constants::PB_LOCKTYPE_ELECTOR())
            .via(eb_)
            .then([this,
                  functionalMembers=values.begin()->second]() mutable {
                switchState_(PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP,
                             folly::sformat("leader id:{} elected", functionalMembers[0].id));
                /* Regardless of what state is reported, for now we will
                 * mark the functionalMembers as functional.  If the member
                 * isn't functional when a message is sent to it will
                 * rejected and the member will go to nonfunctional state
                 */
                for (auto &member : functionalMembers) {
                    member.state = PBMemberState::FOLLOWER_FUNCTIONAL;
                }
                sendBecomeLeaderMsg_(functionalMembers);
            })
            .onError([this](folly::exception_wrapper ew) {
                handleError_(Status::STATUS_INVALID, "remove lock");
            });
    } else {
        switchState_(PBMemberState::EC_WAITING_FOR_QUORUM,
                     "Leader not elected, not enough members for quorum");
        // TODO(Rao): Have some timer based method to redo election to ensure
        // election happens again
    }

}

folly::Future<std::unique_ptr<AddToGroupRespMsg>>
PBMember::handleAddToGroupMsg(std::unique_ptr<AddToGroupMsg> req)
{
    return via(eb_).then([this, req=std::move(req)]() {
        throwIfInvalidTerm_(req->termId);
        THROW_IFNOT_LEADER();
        changePeerState_(req->memberId,
                         req->memberVersion,
                         req->memberState,
                         "add to group request");
        /* Make group level changes if any required */
        auto resp = std::make_unique<AddToGroupRespMsg>();
        resp->syncCommitId = commitId_;
        return resp;
    });
}

void PBMember::handleGroupInfoUpdateMsg(std::unique_ptr<GroupInfoUpdateMsg> req)
{
    via(eb_).then([this, req=std::move(req)]() {
        throwIfInvalidTerm_(req->termId);

        /* If syncing nothing do */
        if (state_ == PBMemberState::FOLLOWER_SYNCING) {
            return;
        }

        /* If not part of functional group go through sync process */
        auto &functionalMembers = req->functionalMembers;
        if (isFollowerState() &&
            std::find_if(functionalMembers.begin(),
                      functionalMembers.end(),
                      [this](const GetMemberStateRespMsg &m) { return m.id == myId_; }) == functionalMembers.end()) {
            leaderId_ = req->leaderId;
            switchState_(PBMemberState::FOLLOWER_SYNCING,
                         "not functional member as per leader");
            if (termId_ < req->termId) {
                termId_ = req->termId;
                CLog(INFO) << " term incremnted to:" << termId_;
            }
            runSyncProtocol();
        } else {
            CVLog(LCONFIG) << " ignoring " << toJsonString(*req);
        }
    });
}

void PBMember::throwIfInvalidTerm_(const int32_t &term)
{
    if (term < termId_) {
        throw StatusException(Status::STATUS_INVALID_TERM);
    }
}

void PBMember::watchCb_(const std::string &key)
{
    auto watchF = [this](const std::string &key) {
        this->watchCb_(key);
    };

    /* Reregister watch for further group changes and get current children */
    auto f = provider_->getCoordinationClient()->getChildrenSimple(groupKey_, watchF);
    f.via(eb_)
    .then([this](const std::vector<std::string>& children) {
        handleGroupWatchEvent(children);
    })
    .onError([this](const StatusException &e) {
        handleError_(e.getStatus(), "watchCb_");
    })
    .onError([this](folly::exception_wrapper ew) {
        handleError_(Status::STATUS_INVALID, "watchCb_");
    });

}

void PBMember::switchState_(PBMemberState newState, const std::string &ctx)
{
    CLog(INFO) << "switch state [" << _PBMemberState_VALUES_TO_NAMES.at(state_)
        << "->" << _PBMemberState_VALUES_TO_NAMES.at(newState) << "]"
        << " - " << ctx;
    state_ = newState;

}

void PBMember::scheduleGroupWatchEvent_()
{
    eb_->runAfterDelay([this]() {
                           CLog(INFO) << "Manual group watch event triggered";
                           watchCb_(groupKey_);
                       },
                       GROUPWATCH_INTERVAL_MS);
}

void PBMember::handleError_(const Status &status,
                            const std::string &ctx)
{
    CHECK(!"Unimplemented");
    // TODO(Rao): 
    // Option1: Based on failure count wait, reset, start the statemachine from member
    // role
    // Option2: Notify parent, so that this member be deleted and new one
    // can be created
}

folly::Future<std::string> PBMember::acquireLock_(const std::string &lockType)
{
    DCHECK(eb_->isInEventBaseThread());
    CLog(INFO) << "Acquiring lock as:" << lockType;
    return provider_->getCoordinationClient()->createEphemeral(
        folly::sformat("{}/{}", groupKey_, pbapi_constants::PB_LOCK_KEY()),
        lockType);
}

folly::Future<folly::Unit> PBMember::removeLock_(const std::string &lockType)
{
    DCHECK(eb_->isInEventBaseThread());
    CLog(INFO) << "Removing lock:" << lockType;
    return provider_->getCoordinationClient()->del(
        folly::sformat("{}/{}", groupKey_, pbapi_constants::PB_LOCK_KEY()), -1);
}

folly::Future<int64_t> PBMember::increaseTerm_()
{
    DCHECK(eb_->isInEventBaseThread());
    CLog(INFO) << "Increasing term";
    return provider_->getCoordinationClient()->set(groupKey_, "", -1);
}

void PBMember::issueElectionRequest_()
{
    DCHECK(eb_->isInEventBaseThread());
    /* Prepare message */
    auto msg = GetMemberStateMsg();
    msg.groupType = configtree_constants::PB_VOLUMES_TYPE();
    msg.termId = termId_;
    msg.resourceId =  resourceId_;

    std::vector<folly::Future<std::unique_ptr<GetMemberStateRespMsg>>> futures;
    /* Send to each child in the group */
    for (const auto &member : memberIds_) {
        auto f = sendKVBMessage<GetMemberStateMsg, GetMemberStateRespMsg>(
            provider_->getConnectionCache(),
            member, 
            msg);
        futures.push_back(std::move(f));
    }

    folly::collectAll(futures)
        .via(eb_)
        .then([this, members=memberIds_](
                const std::vector<folly::Try<std::unique_ptr<GetMemberStateRespMsg>>>& tries){
            std::map<int64_t,
                    std::vector<GetMemberStateRespMsg>> values;
            for (uint32_t i = 0; i < tries.size(); i++) {
                if (tries[i].hasValue()) {
                    auto memberResp = *(tries[i].value());
                    values[memberResp.commitId].push_back(memberResp);
                    CLog(INFO) << "Election response:" << toJsonString(memberResp);
                }
            }
            handleElectionResponse(values);
        });
}

void PBMember::sendBecomeLeaderMsg_(const std::vector<GetMemberStateRespMsg> &functionalMembers)
{
    DCHECK(eb_->isInEventBaseThread());
    /* Prepare message */
    auto msg = BecomeLeaderMsg();
    msg.resourceId = resourceId_;
    msg.termId = termId_;
    msg.functionalMembers = functionalMembers;
    auto f = sendKVBMessage<BecomeLeaderMsg>(
        provider_->getConnectionCache(),
        functionalMembers[0].id, 
        msg);
    CLog(INFO) << "sent BecomeLeaderMsg " << toJsonString(msg);
    /* schedule groupwatch event to be thrown so that in case leader doesnt
     * assume the role, we can retry the election
     */
    scheduleGroupWatchEvent_();
}

bool PBMember::hasLock_(const std::vector<std::string> &children)
{
    return std::find(children.begin(),
                     children.end(),
                     pbapi_constants::PB_LOCK_KEY()) != children.end();
}

std::map<uint64_t, std::string>
PBMember::parseMembers_(const std::vector<std::string> &children)
{
    std::map<uint64_t, std::string> ret;
    std::vector<std::string> members;
    for (const auto &m : children) {
        std::vector<folly::StringPiece> v;
        folly::split(":", m, v);
        if (v.size() == 2) {
            /* v[0] is timestamp v[1] is member id (usally service id) */
            ret[folly::to<uint64_t>(v[1])] = folly::to<std::string>(v[0]);
        }
    }
    return ret;
}

bool PBMember::canIBeElector_(const std::vector<std::string> &children)
{
    if (elapsedTime(lastElectionTimepoint_) < std::chrono::milliseconds(GROUPWATCH_INTERVAL_MS)) {
        return false;
    }
    auto members = parseMembers_(children);
    return (members.size() > 0 && members.begin()->second == myId_);
}

bool PBMember::hasQuorumMemberCount_(const std::vector<std::string> &children)
{
    auto members = parseMembers_(children);
    return members.size() >= quorum_;
}

PBMember::LeaderCtx::PeerInfo* PBMember::getPeerRef_(const std::string &id)
{
    for (auto &peer : leaderCtx_->peers) {
        if (peer.id == id) {
            return &peer;
        }
    }
    return nullptr;
}

std::vector<PBMember::LeaderCtx::PeerInfo> PBMember::getWritablePeers_()
{
    std::vector<PBMember::LeaderCtx::PeerInfo> peers;
    for (const auto &peer : leaderCtx_->peers) {
        if (peer.state == PBMemberState::FOLLOWER_FUNCTIONAL ||
            peer.state == PBMemberState::FOLLOWER_SYNCING) {
            peers.push_back(peer);
        }
    }
    return peers;
}


void PBMember::changePeerState_(const std::string &id,
                                const int64_t &incomingVersion,
                                const PBMemberState &targetState,
                                const std::string &context)
{
    auto member = getPeerRef_(id);
    if (!member) {
        CLog(WARNING) << "Unable to change peer state. Peer with id:" << id
            << " isn't a member. context:" << context;
        return;
    }
    auto currentState = member->state;
    auto currentVersion = member->version;
    bool stateChanged = false;

    /* Adjust member state */
    if (targetState == PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP) {
        if ((currentState == PBMemberState::FOLLOWER_SYNCING ||
            currentState == PBMemberState::FOLLOWER_FUNCTIONAL) 
            && incomingVersion >= currentVersion) {
            member->state = targetState;
            member->version = incomingVersion;
            stateChanged = true;
        }
    } else if (targetState == PBMemberState::FOLLOWER_SYNCING) {
        if ((currentVersion == incomingVersion &&
             currentState == PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP) ||
            incomingVersion > currentVersion) {
            member->state = targetState;
            member->version = incomingVersion;
            stateChanged = true;
        }
    } else if (targetState == PBMemberState::FOLLOWER_FUNCTIONAL) {
        if ((currentVersion == incomingVersion &&
             (currentState == PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP ||
              currentState == PBMemberState::FOLLOWER_SYNCING)) ||
            incomingVersion > currentVersion) {
            member->state = targetState;
            member->version = incomingVersion;
            stateChanged = true;
        }
    }

    if (stateChanged) {
        CLog(INFO) << "member:" << id
            << " state changed from state:" << _PBMemberState_VALUES_TO_NAMES.at(currentState)
            << " version:" << currentVersion
            << " to state:"  << _PBMemberState_VALUES_TO_NAMES.at(targetState)
            << " version:" << incomingVersion;
    } else {
        CVLog(LCONFIG) << "member:" << id
            << " state change IGNORED from state:" << _PBMemberState_VALUES_TO_NAMES.at(currentState)
            << " version:" << currentVersion
            << " to state:"  << _PBMemberState_VALUES_TO_NAMES.at(targetState)
            << " version:" << incomingVersion;
    }

    uint32_t nFunctional = 0;
    uint32_t nSyncing = 0;
    uint32_t nNonFunctional = 0;
    std::for_each(leaderCtx_->peers.begin(),
                  leaderCtx_->peers.end(),
                  [&nFunctional, &nSyncing, &nNonFunctional](const LeaderCtx::PeerInfo &p) {
                      if (p.state == PBMemberState::FOLLOWER_FUNCTIONAL) {
                          nFunctional++;
                      } else if (p.state == PBMemberState::FOLLOWER_SYNCING) {
                          nSyncing++;
                      } else {
                          nNonFunctional++;
                      }
                  });
    /* Adjust group/leader state */
    // TODO(Rao): Publish groupstate change to configdb
    if (state_ == PBMemberState::LEADER_FUNCTIONAL && nFunctional < quorum_-1) {
        switchState_(PBMemberState::LEADER_WAITING_FOR_QUORUM, "change peer state");
    } else if (state_ == PBMemberState::LEADER_WAITING_FOR_QUORUM && nFunctional >= quorum_-1) {
        switchState_(PBMemberState::LEADER_FUNCTIONAL, "change peer state");
    }

    DCHECK((nFunctional+nNonFunctional+nSyncing) == leaderCtx_->peers.size());
    DCHECK(nFunctional < quorum_-1 || state_ == PBMemberState::LEADER_FUNCTIONAL);
}

folly::Future<folly::Unit> PBMember::writeToPeers(const std::string &type,
                                                  std::unique_ptr<folly::IOBuf> buffer) {
    struct WriteCtx {
        folly::Promise<folly::Unit> promise;
        uint32_t nAcked {0};
        uint32_t nSuccess {0};
        uint32_t nPeers;
    };

    DCHECK(state_ == PBMemberState::LEADER_FUNCTIONAL); 

    auto connMgr = provider_->getConnectionCache();
    auto peers = getWritablePeers_();
    auto writeCtx = std::make_shared<WriteCtx>();
    writeCtx->nPeers = peers.size();
    CVLog(LIO) << "created write context:" << writeCtx.get() << " nPeers:" << peers.size();

    for (const auto &peer : peers) {
        auto reqKvb = std::make_unique<KVBuffer>();
        setType(*reqKvb, type);
        reqKvb->payload = buffer->clone();
        // TODO(Rao): Set timeout
        auto f = 
            connMgr
            ->getAsyncClient<ServiceApiAsyncClient>(peer.id)
            .then([this, reqKvb=std::move(reqKvb),
                  to=peer.id](const std::shared_ptr<ServiceApiAsyncClient>& client) {
                CVLog(LIO) << "Sending KVBinary type:" << getType(*reqKvb)
                        << " to:" << to;
                return client->future_handleKVBMessage(*reqKvb);
            })
            .via(eb_)
            .then([writeCtx, peerPrior=peer, this](const folly::Try<KVBuffer> &respTry) {
                  DCHECK(eb_->isInEventBaseThread());
                  // TODO(Rao):
                  // 1. handle when we aren't leader anymore
                  // 2. Trace information
                  // 3: Handling cases when functional members count < quorum
                  // etc.
                  auto peer = getPeerRef_(peerPrior.id);
                  writeCtx->nAcked++;
                  if (respTry.hasValue()) {
                      CVLog(LIO) << "Received KVBinary type:" << getType(respTry.value())
                          << " from:" << peerPrior.id;
                      writeCtx->nSuccess++;
                      if (writeCtx->nSuccess == quorum_-1 &&
                          !writeCtx->promise.isFulfilled()) {
                          writeCtx->promise.setValue(folly::Unit());
                      }
                  } else {
                      CVLog(LIO) << "Received KVBinary exception:"
                          << respTry.exception().what()
                          << " from:" << peerPrior.id;
                      changePeerState_(peer->id,
                                       peerPrior.version,
                                       PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP,
                                       folly::sformat("write failed exception:{}",
                                                      respTry.exception().what()));
                      if (writeCtx->nAcked == writeCtx->nPeers &&
                          !writeCtx->promise.isFulfilled()) {
                          writeCtx->promise.setException(respTry.exception());
                      }
                  }
                  CVLog(LIO) << "write context:" << writeCtx.get()
                      << " nAcked:" << writeCtx->nAcked
                      << " nSuccess:" << writeCtx->nSuccess;
            });
    }
    return writeCtx->promise.getFuture();
}

}  // namespace infra
