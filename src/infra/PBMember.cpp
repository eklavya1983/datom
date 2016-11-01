#include <folly/futures/Future.h>
#include <folly/futures/Future-inl.h>

#include <infra/PBRoleCoordinator.h>
#include <infra/CoordinationClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>

namespace infra {

PBRole::PBRole(PBMember *parent,
               RoleId roleId)
    : parent_(parent),
    roleId_(roleId)
{
}

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

PBMember::PBMember(folly::EventBase *eb,
                   CoordinationClient *coordinationClient,
                   const std::string &groupKey,
                   const std::vector<std::string> &members,
                   const std::string &myId)
    : eb_(eb),
    coordinationClient_(coordinationClient),
    groupKey_(groupKey),
    memberIds_(members),
    myId_(myId)
{
}

void PBMember::init()
{
    switchRole_(makeRole<PBFollower>(), State::FOLLOWER_WAIT_TO_JOIN_GROUP, __FUNCTION__);

    /* Register watch on group key */
    coordinationClient_->registerWatch(
        groupKey_,
        [this](const std::string &key) {
            auto f = coordinationClient_->getChildrenSimple(groupKey_);
            f.then(eb_, [this](const std::vector<std::string>& children) {
                        role_->handleGroupWatchEvent(children);
                   });
        });
}

void PBMember::switchRole_(PBRole *newRole, State state, const std::string &ctx)
{
    role_.reset(newRole);
    state_ = state;
}



}  // namespace infra
