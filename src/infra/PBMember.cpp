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
#include <infra/StatusException.h>
#include <infra/typestr.h>
#include <infra/MessageUtils.tcc>
#include <infra/ConnectionCache.tcc>


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

namespace infra {

PBMember::PBMember(const std::string &logCtx,
                   folly::EventBase *eb,
                   ModuleProvider *provider,
                   const std::string &groupKey,
                   const std::vector<std::string> &members,
                   const std::string &myId,
                   const uint32_t &quorum)
    : logContext_(logCtx),
    eb_(eb),
    provider_(provider),
    groupKey_(groupKey),
    memberIds_(members),
    myId_(myId),
    quorum_(quorum)
{
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

    /* Create sequential ephemeral node for this member */
    auto memberRoot = folly::sformat("{}/{}", groupKey_, myId_);
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
    switch (state_) {
        case PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP:
            if (!hasLock_(children) &&
                canIBeElector_(children))  {
                switchState_(PBMemberState::EC_ACQUIRE_LOCK, "handleGroupWatchEvent");
                auto f = acquireElectorLock_();
                f
                .via(eb_)
                .then([this]() {
                    DCHECK(state_ == PBMemberState::EC_ACQUIRE_LOCK);
                    return increaseTerm_();
                })
                .then([this](int64_t term) {
                    DCHECK(state_ == PBMemberState::EC_ACQUIRE_LOCK);
                    termId_ = term;
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

folly::Future<GetMemberStateRespMsg>
PBMember::handleGetMemberStateMsg(const GetMemberStateMsg &req)
{
    throwIfInvalidTerm_(req.termId);

    GetMemberStateRespMsg resp;
    resp.commitId = commitId_;

    return folly::makeFuture(resp);
}

folly::Future<folly::Unit>
PBMember::handleBecomeLeaderMsg(const BecomeLeaderMsg &req)
{
    throwIfInvalidTerm_(req.termId);
    
    switchState_(PBMemberState::LEADER_BEGIN,
                 "Become leader message");

    return folly::makeFuture();
}

void
PBMember::handleElectionResponse(
    std::vector<std::pair<std::string, GetMemberStateRespMsg>> values)
{
    DCHECK(eb_->isInEventBaseThread());
    DCHECK(state_ == PBMemberState::EC_ELECTION_IN_PROGRESS);
    /* Determine leader */
    /* For now leader is the entity with latest state.  TODO(Rao): This needs to be
     * imporved so that we take term into account as well
     */
    if (values.size() >= quorum_) {
        uint32_t maxIdx = 0;
        for (uint32_t i = 1; i < values.size(); i++) {
            if (values[i].second.commitId > values[maxIdx].second.commitId) {
                maxIdx = i;
            }
        }
        /*
         * Remove lock.  Remove lock before switching to follower role as
         * removing lock will trigger handleGroupWatchEvent and if we are in
         * in member role, we could re-acquire the lock
         */
        removeElectorLock_()
            .via(eb_)
            .then([this,
                  leader=values[maxIdx].first,
                  commitId=values[maxIdx].second.commitId]() {
                switchState_(PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP,
                             folly::sformat("leader id:{} elected", leader));
                sendBecomeLeaderMsg_(leader, commitId);
            });
    } else {
        switchState_(PBMemberState::FOLLOWER_WAIT_TO_JOIN_GROUP,
                     "Leader not elected, not enough members for quorum");
        // TODO(Rao): Have some timer based method to redo election to ensure
        // election happens again
    }

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
    // TODO(Rao) : Log
    CLog(INFO) << "switch state [" << _PBMemberState_VALUES_TO_NAMES.at(state_)
        << "->" << _PBMemberState_VALUES_TO_NAMES.at(newState) << "]"
        << " - " << ctx;
    state_ = newState;
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

folly::Future<std::string> PBMember::acquireElectorLock_()
{
    DCHECK(eb_->isInEventBaseThread());
    return provider_->getCoordinationClient()->createEphemeral(
        folly::sformat("{}/{}", groupKey_, pbapi_constants::PB_LOCK_KEY()),
        "elector");
}

folly::Future<folly::Unit> PBMember::removeElectorLock_()
{
    DCHECK(eb_->isInEventBaseThread());
    CLog(INFO) << "Removing elector lock";
    return provider_->getCoordinationClient()->del(
        folly::sformat("{}/{}", groupKey_, pbapi_constants::PB_LOCK_KEY()), -1);
}

folly::Future<int64_t> PBMember::increaseTerm_()
{
    DCHECK(eb_->isInEventBaseThread());
    return provider_->getCoordinationClient()->set(groupKey_, "", -1);
}

void PBMember::issueElectionRequest_()
{
    DCHECK(eb_->isInEventBaseThread());
    /* Prepare message */
    auto msg = GetMemberStateMsg();

    std::vector<folly::Future<GetMemberStateRespMsg>> futures;
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
                const std::vector<folly::Try<GetMemberStateRespMsg>>& tries){
            std::vector<std::pair<std::string, GetMemberStateRespMsg>> values;
            for (uint32_t i = 0; i < tries.size(); i++) {
                if (tries[i].hasValue()) {
                    values.push_back(std::make_pair(members[i], std::move(tries[i].value())));
                }
            }
            handleElectionResponse(values);
        });
}

void PBMember::sendBecomeLeaderMsg_(const std::string &memberId,
                                    const int64_t &commitId)
{
    DCHECK(eb_->isInEventBaseThread());
    /* Prepare message */
    auto msg = BecomeLeaderMsg();
    msg.termId = termId_;
    msg.commitId = commitId;
    auto f = sendKVBMessage<BecomeLeaderMsg>(
        provider_->getConnectionCache(),
        memberId, 
        msg);
    CLog(INFO) << "sent BecomeLeaderMsg " << toJsonString(msg);
    // TODO(Rao):
    // Handle when case leader doesn't show up evenn after become leader has
    // been sent
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
    auto members = parseMembers_(children);
    return (members.size() > 0 && members.begin()->second == myId_);
}

bool PBMember::hasQuorumMemberCount_(const std::vector<std::string> &children)
{
    auto members = parseMembers_(children);
    return members.size() >= quorum_;
}

}  // namespace infra
