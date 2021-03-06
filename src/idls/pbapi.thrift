include "commontypes.thrift"

namespace cpp infra 

const string PB_LOCK_KEY="LOCK"
const string PB_LOCKTYPE_ELECTOR="elector"
const string PB_LOCKTYPE_LEADER="leader"

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
enum PBMemberState {
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
}

struct GetMemberStateMsg {
    1: string		groupType;
    2: i64		resourceId;
    3: i32		termId;
}

struct GetMemberStateRespMsg {
    1: string           id;
    2: i64 		commitId;
    3: i64 		version;
    4: PBMemberState 	state;
}

/**
* @brief Message sent from elector post election to the leader  to assume leader role
*/
struct BecomeLeaderMsg {
    1: i64		resourceId;
    2: i32		termId;
    3: list<GetMemberStateRespMsg> functionalMembers;
}

/**
* @brief Message sent for leader to follower to notify current group members.  Based on
* this message non-functional members will attempt to join back in the group via
* sync protocol
*/
struct GroupInfoUpdateMsg {
    1: i64		resourceId;
    2: i32		termId;
    3: string    	leaderId;
    4: list<GetMemberStateRespMsg> functionalMembers;
}

/**
* @brief Message from member to leader to be added into the group.  Depending on reported 
* member state leader updates its perspective and takes necessary action
*/
struct AddToGroupMsg {
    1: i64		resourceId;
    2: i32		termId;
    3: string 		memberId;
    4: i64 		memberVersion;	
    /* State the member currently is in from member's perspective */
    5: PBMemberState 	memberState;
}

/**
* @brief Response to AddToGroupMsg
*/
struct AddToGroupRespMsg {
    /* Membmer will need to sync upto this point before it can become functional */
    1: i64 		syncCommitId;
}

/**
* @brief Message to pull journal entries
*/
struct PullJournalEntriesMsg {
    1: i64		resourceId;
    2: i32		termId;
    3: i64  		fromId;
    4: i64 		toId;
    /* Maximum bytes to pack in the response.  NOTE: It should be >= maximum size of a journal entry */
    5: i64 		maxBytesInResp;
    /* When set, joural entries [fromId, toId] will be locked for specified seconds */
    6: i32 		lockTimeInSec;
}

/**
* @brief Response for pull journal entries request
*/
struct PullJournalEntriesRespMsg {
    1: i64  		fromId;
    2: i64 		toId;
    3: list<commontypes.JournalEntry> journalEntries;
}

