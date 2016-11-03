#include <folly/io/async/EventBase.h>
#include <folly/futures/Future.h>
#include <folly/futures/Future-inl.h>
#include <folly/String.h>
#include <folly/Format.h>

#include <infra/PBMember.h>
#include <infra/CoordinationClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/StatusException.h>
#include <infra/gen/gen-cpp2/pbapi_constants.h>

namespace infra {

PBRole::PBRole(PBMember *parent,
               RoleId roleId)
    : parent_(parent),
    roleId_(roleId)
{
}

void PBRole::handleGroupWatchEvent(const std::vector<std::string>& children)
{
}

#if 0
struct PBFollower;
struct PBElector;
struct PBLeader;

struct PBFollower : PBRole {
    PBFollower(PBMember *parent)
        : PBRole(parent, RoleId::FOLLOWER)
    {
    }
};

struct PBElector : PBRole {
    PBElector(PBMember *parent)
        : PBRole(parent, RoleId::ELECTOR)
    {
    }

};

struct PBLeader : PBRole {
    PBLeader(PBMember *parent)
        : PBRole(parent, RoleId::LEADER)
    {
    }

};
#endif

PBMember::PBMember(folly::EventBase *eb,
                   CoordinationClient *coordinationClient,
                   const std::string &groupKey,
                   const std::vector<std::string> &members,
                   const std::string &myId,
                   const uint32_t &quorum)
    : eb_(eb),
    coordinationClient_(coordinationClient),
    groupKey_(groupKey),
    memberIds_(members),
    myId_(myId),
    quorum_(quorum)
{
}

void PBMember::init()
{
    switchState_(State::FOLLOWER_WAIT_TO_JOIN_GROUP, __FUNCTION__);

    /* Register watch for group changes */
    auto watchF = [this](const std::string &key) {
        watchCb_(key);
    };
    /* Though we don't care for getting children here, this is how we register
     * watch on the group
     */
    coordinationClient_->getChildrenSimple(groupKey_, watchF);

    /* Create sequential ephemeral node for this member */
}

void PBMember::handleGroupWatchEvent(const std::vector<std::string> &children)
{
    switch (state_) {
        case FOLLOWER_WAIT_TO_JOIN_GROUP:
            if (!hasLock_(children) &&
                canIBeElector_(children))  {
                switchState_(PBMember::EC_ACQUIRE_LOCK, "handleGroupWatchEvent");
                auto f = acquireElectorLock_();
                f
                .via(eb_)
                .then([this]() {
                    DCHECK(state_ == PBMember::EC_ACQUIRE_LOCK);
                    return coordinationClient_->getChildrenSimple(groupKey_);
                })
                .then([this](const std::vector<std::string> &children) {
                    DCHECK(state_ == PBMember::EC_ACQUIRE_LOCK);
                    DCHECK(hasLock_(children));
                    switchState_(EC_WAITING_FOR_QUORUM, "lock acquired");
                    if (hasQuorumMemberCount_(children)) {
                        switchState_(EC_ELECTION_IN_PROGRESS, "quorum met");
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
        case EC_WAITING_FOR_QUORUM:
            DCHECK(hasLock_(children));
            if (hasQuorumMemberCount_(children)) {
                switchState_(EC_ELECTION_IN_PROGRESS, "quorum met");
                issueElectionRequest_();
            }
            break;
        default:
            break;
    }
}

void PBMember::watchCb_(const std::string &key)
{
    auto watchF = [this](const std::string &key) {
        this->watchCb_(key);
    };

    /* Reregister watch for further group changes and get current children */
    auto f = coordinationClient_->getChildrenSimple(groupKey_, watchF);
    f.via(eb_)
    .then([this](const std::vector<std::string>& children) {
        role_->handleGroupWatchEvent(children);
    })
    .onError([this](const StatusException &e) {
        handleError_(e.getStatus(), "watchCb_");
    })
    .onError([this](folly::exception_wrapper ew) {
        handleError_(Status::STATUS_INVALID, "watchCb_");
    });

}

void PBMember::switchState_(State newState, const std::string &ctx)
{
    // TODO(Rao) : Log
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
    return coordinationClient_->createEphemeral(
        folly::sformat("{}/{}", groupKey_, pbapi_constants::PB_LOCK_KEY()),
        "elector");
}

void PBMember::issueElectionRequest_()
{
#if 0
    DCHECK(eb_->isInEventBaseThread());
    /* Prepare message */
    auto msg = GetMemberStateMsg();

    /* Send to each child in the group */
    for (const auto &member : memberIds_) {
        auto f = 
            connectionCache_
            .getClient<ServiceApiAsyncClient>(member)
            .then([](const ServiceApiAsyncClientPtr& client) {
                  return client->future_handleKVBMessage();
            })
            .within(milliseconds(5000));
        futures.push_back(f);
    }

    folly::collectAll(futures)
        .then([](const std::vector<folly::Try<GetMemberStateRespMsg>>& tries){
            for (const auto &t : tries) {
                if (t.hasValue()) {
                    values.push_back(t.value());
                }
            }
        });
#endif
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
