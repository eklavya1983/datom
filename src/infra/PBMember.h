#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <infra/InfraForwards.h>
#include <infra/gen/gen-cpp2/pbapi_types.h>
#include <infra/TimeUtil.h>

#define THROW_IFNOT_LEADER() \
    if (!isLeaderState() || !leaderCtx_) { \
        CLog(WARNING) << "Member isn't a leader"; \
        throw StatusException(Status::STATUS_NOT_LEADER); \
    }

namespace folly {
class EventBase;
}

namespace infra {
class GetMemberStateMsg;
class GetMemberStateRespMsg;
class BecomeLeaderMsg;
struct ModuleProvider;
struct PBMember;
enum class Status;

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

#if 0
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
        FOLLOWER_BEGIN,
        FOLLOWER_WAIT_TO_JOIN_GROUP,
        FOLLOWER_SYNCING,
        FOLLOWER_FUNCTIONAL,
        FOLLOWER_END,

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
        EC_BEGIN,
        EC_ACQUIRE_LOCK,
        EC_WAITING_FOR_QUORUM,
        EC_ELECTION_IN_PROGRESS,
        EC_END,

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
        LEADER_BEGIN,
        LEADER_WAITING_FOR_QUORUM,
        LEADER_FUNCTIONAL,
        LEADER_END
    };
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
#endif

    PBMember(const std::string &logCtx,
             folly::EventBase *eb,
             ModuleProvider *provider,
             int64_t resourceId,
             const std::string &groupKey,
             const std::vector<std::string> &members,
             const std::string &myId,
             const uint32_t &quorum);
    void init();
    folly::EventBase* getEventBase() { return eb_; }

    /* Events to handle */
    virtual void handleGroupWatchEvent(const std::vector<std::string> &children);
    virtual folly::Future<std::unique_ptr<GetMemberStateRespMsg>>
        handleGetMemberStateMsg(std::unique_ptr<GetMemberStateMsg> req);
    virtual void handleElectionResponse(const std::map<int64_t,
                                std::vector<GetMemberStateRespMsg>> &values);
    virtual void handleBecomeLeaderMsg(std::unique_ptr<BecomeLeaderMsg> req);
    virtual folly::Future<std::unique_ptr<AddToGroupRespMsg>>
        handleAddToGroupMsg(std::unique_ptr<AddToGroupMsg> req);
    virtual void handleGroupInfoUpdateMsg(std::unique_ptr<GroupInfoUpdateMsg> req);

     /* Methods to override */
    virtual void runSyncProtocol() {}

    inline const PBMemberState& getState() const { return state_; }

    inline bool isFollowerState() const {
        return state_ > PBMemberState::FOLLOWER_BEGIN &&
            state_ < PBMemberState::FOLLOWER_END;
    }
    inline bool isElectorState() const {
        return state_ > PBMemberState::EC_BEGIN &&
            state_ < PBMemberState::EC_END;
    }
    inline bool isLeaderState() const {
        return state_ > PBMemberState::LEADER_BEGIN &&
            state_ < PBMemberState::LEADER_END;
    }
    const std::string& getLogContext() const {
        return logContext_;
    }

    /**
     * @brief Leader context.  Used when member is a leader
     */
    struct LeaderCtx {
        LeaderCtx();

        struct PeerInfo {
            std::string         id;
            PBMemberState       state;
            int64_t             version;
        };

        int64_t                                 opId; 
        int64_t                                 commitId;
        std::vector<PeerInfo>                   peers;
    };

    folly::Future<folly::Unit> writeToPeers(const std::string &type,
                                            std::unique_ptr<folly::IOBuf> buffer);

    static const int32_t GROUPWATCH_INTERVAL_MS;

 protected:
    void handleError_(const Status &status, const std::string &ctx);
    void watchCb_(const std::string &key);
    void scheduleGroupWatchEvent_();
    void throwIfInvalidTerm_(const int32_t &term);
    
    void switchState_(PBMemberState newState, const std::string &ctx);

    std::map<uint64_t, std::string> parseMembers_(const std::vector<std::string> &children);
    folly::Future<std::string> acquireLock_(const std::string &lockType);
    folly::Future<folly::Unit> removeLock_(const std::string &lockType);
    folly::Future<int64_t> increaseTerm_();
    void issueElectionRequest_();
    void sendBecomeLeaderMsg_(const std::vector<GetMemberStateRespMsg> &functionalMembers);

    bool hasLock_(const std::vector<std::string> &children);
    bool canIBeElector_(const std::vector<std::string> &children);
    bool hasQuorumMemberCount_(const std::vector<std::string> &children);


    // methods available in leader role
    LeaderCtx::PeerInfo* getPeerRef_(const std::string &id);
    std::vector<LeaderCtx::PeerInfo> getWritablePeers_();
    template<class ReqT, class RespT, typename F>
    folly::Future<std::unique_ptr<RespT>> groupWriteInEb_(F &&localWriteFunc,
                                                          std::unique_ptr<ReqT> msg);
    void changePeerState_(const std::string &id,
                          const int64_t &incomingVersion,
                          const PBMemberState &targetState,
                          const std::string &context);

    std::string                         logContext_;
    folly::EventBase                    *eb_;
    ModuleProvider                      *provider_;
    int64_t                             resourceId_;
    std::string                         groupKey_;
    std::vector<std::string>            memberIds_;
    std::string                         myId_;
    std::string                         leaderId_;
    uint32_t                            quorum_;
    PBMemberState                       state_ {PBMemberState::UNINITIALIZED};
    int64_t                             version_;

    /* TermId is the version # of groupKey_ in config db.  We don't have a
     * seperate key as groupKey_ version # is sufficient and configdb will
     * ensure atomic increment as each time groupKey_ is set.
     * Term id is incremented for every leader election
     */
    int32_t                             termId_;
    TimePoint                           lastElectionTimepoint_; 
    int64_t                             opId_;
    int64_t                             commitId_;
    std::unique_ptr<LeaderCtx>          leaderCtx_;
};

}  // namespace infra
