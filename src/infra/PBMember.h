#pragma once

#include <string>
#include <vector>
#include <memory>

namespace folly {
class EventBase;
}

namespace infra {

struct CoordinationClient;
struct PBMember;

/**
 * @brief Base class for diffent roles PBMember can take
 * We do mostly state transition based on events stuff in derived role classes.
 * Try not to add complex members here or in dervied classes.  They belong in
 * PBMember class.  This is to allow quick role switching.
 */
struct PBRole {
    enum RoleId {
        FOLLOWER,
        ELECTOR,
        LEADER
    };

    PBRole(PBMember *parent,
           RoleId roleId);
    virtual ~PBRole() = default;
    inline RoleId getRoleId() const { return roleId_; }

    /**
     * Possible events
     * Zookeeper watches
     * GroupUpdateMsg
     * ElectionMsg
     * BecomeLeaderMsg
     * AddToGroupMsg
     * IO Msgs
     * Sync Msgs
     */
    virtual void handleGroupWatchEvent(const std::vector<std::string>& children) = 0;
 protected:
    PBMember                    *parent_;
    RoleId                      roleId_;
};

struct PBMember {

    /**
     * Following is the group's lifecycle
     * {1: No leader} -> {2: wait for quorum} -> {3: elect leader} -> {4. leader in charge}
     * At a group level coordination there are two roles.  Leader
     * role and Elector role.  This role transition is coordinated via
     * coordination lock to gaurantee exclusivity
     * When there is no leader first member of the group assumes Elector role
     * and coordinates the group through steps 1, 2, 3.  Once leader is elected
     * coordination role is given to Leader to carry out step 4.
     */

    /**
     * Possible events
     * Zookeeper watches
     * GroupUpdateMsg
     * ElectionMsg
     * BecomeLeaderMsg
     * AddToGroupMsg
     * IO Msgs
     * Sync Msgs
     */
    enum State {
        UNINITIALIZED = 0,

        /* As a member the goal is become part of the group.  In FOLLOWER_WAIT_TO_JOIN_GROUP
         * state we wait to be contacted by leader to know the group state and either
         * become part of the group immediately or go throgh sync before
         * becoming part of the group.
         * If there is no leader, member can take on the role
         * of ELECTOR in order to elect leader for the group.
         * When member is FOLLOWER_FUNCTIONAL on election message for a new term
         * member will go back to FOLLOWER_WAIT_TO_JOIN_GROUP state
         * On request to become leader for a term, if member is in
         * FOLLOWER_WAIT_TO_JOIN_GROUP state, it takes on the role of LEADER
         */
        /* From                          EVENT                           NEXT                            ACTION
         * FOLLOWER_WAIT_TO_JOIN_GROUP  |No leader & no quorum          |EC_WAITING_FOR_QUORUM          |coordination lock & increase term #
         *                               & not waiting for leader
         * FOLLOWER_WAIT_TO_JOIN_GROUP  |No leader & quorum             |EC_ELECTION_IN_PROGRESS        |coordination lock & increase term # & send election message
         *                               & not waiting for leader
         * FOLLOWER_WAIT_TO_JOIN_GROUP  |GroupupdateMsg with            |FOLLOWER_FUNCTIONAL            |
         *                               me as member 
         * FOLLOWER_WAIT_TO_JOIN_GROUP  |GroupupdateMsg with            |FOLLOWER_SYNCING               |
         *                               me as NOT functional member 
         * FOLLOWER_FUNCTIONAL          |GroupupdateMsg with            |FOLLOWER_SYNCING               |
         *                               me as NOT functional member 
         *       *                      |ZK event(No leader)            |FOLLOWER_WAIT_TO_JOIN_GROUP    |
         *       *                      |Election event with higher term|FOLLOWER_WAIT_TO_JOIN_GROUP    | increase term #
         */
        FOLLOWER_WAIT_TO_JOIN_GROUP,
        FOLLOWER_SYNCING,
        FOLLOWER_FUNCTIONAL,

        /* When there is no leader member takes on the role of
         * ELECTOR.  In this role we first take the group 
         * coordination lock set status to electing then go to
         * EC_WAITING_FOR_QUORUM state and wait for quorum # of
         * members to be available before doing the actual election.
         * During election we stay in EC_ELECTION_IN_PROGRESS role
         * At the end of election, after a leader is selected we go
         * into FOLLOWER_WAIT_TO_JOIN_GROUP state before sending
         * become leader message to new leader
         * If the leader doesnt appear in certain amount of time, member
         * should take on ELECTOR role and resume election with
         * a new term
         */
        /* From                          EVENT                           NEXT                            ACTION
         * EC_WAITING_FOR_QUORUM        |quorum # of members            |EC_ELECTION_IN_PROGRESS        |send election message
         * EC_ELECTION_IN_PROGRESS      |respnsees recvd & leader found |FOLLOWER_WAIT_TO_JOIN_GROUP    |wait for leader to showup on a timer
         */
        EC_WAITING_FOR_QUORUM,
        EC_ELECTION_IN_PROGRESS,

        /* On message to become leader for new term, we take on the LEADER role.
         * Depending upon avialable # of members we can start with either
         * LEADER_WAITING_FOR_QUORUM or LEADER_FUNCTIONAL state
         * When a member joins we may transition from LEADER_WAITING_FOR_QUORUM
         * to LEADER_FUNCTIONAL.
         * When a member leaves (possibly member crashed etc.) we may transition
         * from LEADER_FUNCTIONAL to LEADER_WAITING_FOR_QUORUM
         * At regual intervals we will broadcast out group information so that
         * members can join the group if need be
         */
        /* From                          EVENT                           NEXT                            ACTION
         * LEADER_FUNCTIONAL            |< quorum # of members via zkevt|LEADER_WAITING_FOR_QUORUM      |
         *                               or io event
         * LEADER_WAITING_FOR_QUORUM    |AddToGroupMsg && quorum met    |LEADER_FUNCTIONAL              |
         */
        LEADER_WAITING_FOR_QUORUM,
        LEADER_FUNCTIONAL,
    }

    void algo() {
        zk->registerWatch(GROUP_PATH, cb);
        zk->createNode(SEQUNETIAL|EPHEMERAL, myuuid);
        auto cb = []() {
            auto f = zk->getChildren();
            f.then();
        };
    }

    void handleGroupWatchEvent(std::vector<std::string> members)
    {
        if (role_ == FOLLOWER) {
            if (state_ == UNINITIALIZED) {
                if (isElectionCoordinator(members)) {
                    role_ = ELECTOR;
                    if (members.size() < quorum_)  {
                        state_ = EC_WAITING_FOR_QUORUM;
                    } else {
                        sendElectionRequest_();
                        state_ = EC_ELECTION_IN_PROGRESS;
                    }
                } else {
                    state_ = FOLLOWER_WAIT_TO_JOIN_GROUP;
                }
            }
        } else if (role_ == ELECTOR) {
            if (state_ == EC_WAITING_FOR_QUORUM &&
                members.size() >= quorum_) {
                sendElectionRequest_();
                state_ = EC_ELECTION_IN_PROGRESS;
            }
        }
    }

    void handleElectionMsg_(const ElectionMsg &msg)
    {
        if (msg.term < term_) {
            reject(STATUS_INVALID_TERM);
        }

        CHECK(role_ != ELECTOR);

        if (role_ != FOLLOWER) {
            CLog(INFO) << "changing role to member from:" << role_;
            role_ = FOLLOWER;
            state_ = UNINITIALIZED;
        }
        term_ = msg.term;

        auto resp = std::make_shared<ElectionRespMsg>();
        resp.commitId = commitId_;
        reply(resp);
    }

    void handleElectionRespMsg_(const ElectionRespMsg& msg)
    {
        if (msg.term < term_) {
            CLog(WARNING) << "Invalid current term: " << term_
                << " incoming term:" << msg.term;
            return;
        } else if (role_ != ELECTOR)  {
            return;
        }

        /* TODO(rao): Update election context */
        if (leaderSelected) {
            announceLeader();
            role_ = FOLLOWER;
            state_ = FOLLOWER_WAIT_TO_JOIN_GROUP;
        }
    }

    void handleGroupUpdateMsg_(const GroupUpdate &msg)
    {
    }

    PBMember(folly::EventBase *eb,
             CoordinationClient *coordinationClient,
             const std::string &groupKey,
             const std::vector<std::string> &members,
             const std::string &myId);
    void init();
    bool amILeader();

    template<class T>
    T* makeRole()
    {
        return new T(this);
    }

 protected:
    void switchRole_(PBRole *newRole, State state, const std::string &ctx);

    folly::EventBase                    *eb_;
    CoordinationClient                  *coordinationClient_;
    std::string                         groupKey_;
    std::vector<std::string>            memberIds_;
    std::string                         myId_;
    std::unique_ptr<PBRole>             role_;
    State                               state_ {UNINITIALIZED};
};

}  // namespace infra
