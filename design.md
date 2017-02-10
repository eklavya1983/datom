#Infra modules 
##Adding nodes
-We will have addnode script
-This command will be run from the node itself
-It will do hardward discovery and report to config service
-It will also configure services to run
-After node is added to via config service, infromation returned from config service is stamped on the node

##Service bootup
-Service will have following args
--nic_if, root, port, domain_name, service_name
-At root we expect json file with node information validated by config service.  This will contain node name for now
-Service init the following
-Coordination client
-Properties
-Connection cache
--expose getConnection().  Will first look in cache followed by zk. Returns future.
--register for updates for any connection in connection cache
-Create znode
-Publish it's up

##Connection cache
-Connection cache runs in it's own eventbase
-Each connection also runs in it's own eventbase which is given to the async socket.
-Connectin cache manages a bunch of TAsyncSockets in cache.  On updates from coordination client to cached sockets will updated.

#Replication

##Primary backup protocol

Following is the group's lifecycle
{1: No leader} -> {2: wait for quorum} -> {3: elect leader} -> {4. leader in charge}
At a group level coordination there are two roles.  Leader
role and Elector role.  This role transition is coordinated via
coordination lock to gaurantee exclusivity
When there is no leader first member of the group assumes Elector role
and coordinates the group through steps 1, 2, 3.  Once leader is elected
coordination role is given to Leader to carry out step 4.

**Follower role**
States:
FOLLOWER_WAIT_TO_JOIN_GROUP
FOLLOWER_SYNCING
FOLLOWER_FUNCTIONAL

As a member the goal is become part of the group.  In FOLLOWER_WAIT_TO_JOIN_GROUP state we wait to be contacted by leader to know the group state and either become part of the group immediately or go throgh sync before becoming part of the group. If there is no leader, member can take on the role of ELECTOR in order to elect leader for the group.  When member is FOLLOWER_FUNCTIONAL on election message for a new term member will go back to FOLLOWER_WAIT_TO_JOIN_GROUP state On request to become leader for a term, if member is in FOLLOWER_WAIT_TO_JOIN_GROUP state, it takes on the role of LEADER

**Elector role**
States:
EC_ACQUIRE_LOCK
EC_WAITING_FOR_QUORUM,
EC_ELECTION_IN_PROGRESS,

When there is no leader member takes on the role of ELECTOR.  In this role we first take the grou coordination lock set status to electing then go to EC_WAITING_FOR_QUORUM state and wait for quorum # of members to be available before doing the actual election.  During election we stay in EC_ELECTION_IN_PROGRESS role At the end of election, after a leader is selected we go into FOLLOWER_WAIT_TO_JOIN_GROUP state before sending become leader message to new leader If the leader doesnt appear in certain amount of time, member should take on ELECTOR role and resume election with a new term.

**Leader role**
States:
LEADER_WAITING_FOR_QUORUM
LEADER_FUNCTIONAL

On message to become leader for new term, we take on the LEADER role. Depending upon avialable # of members we can start with either LEADER_WAITING_FOR_QUORUM or LEADER_FUNCTIONAL state.  When a member joins we may transition from LEADER_WAITING_FOR_QUORUM to LEADER_FUNCTIONAL. When a member leaves (possibly member crashed etc.) we may transition from LEADER_FUNCTIONAL to LEADER_WAITING_FOR_QUORUM At regual intervals we will broadcast out group information so that members can join the group if need be.

**Locking**
The entity that is coordinating the group acquires lock.  Lock is acquired by either an elector or a leader in their respective roles to acheive role coordination exclusivity.  Lock held is ephemeral and will go away when entity holding the lock goes away.

**Term significance**
//TODO


#Config service
-First the user will have to create datom tree.  This will create a node with config serivce as an entry in the tree.
-When config service comes up it checks if path /datom/configservice exists.  If it exits, then it's an existing dataom instance.  Otherwise it's a fresh instance.
-Under /datom/configserivce/instance will be a znode that stays up until config service stays up
Handling errors
-We will return exceptions.  That way it's possible include descriptive error messages as well.  Same can be done with status code also.  But it'd require creating an error code for each case.


#Volume server
-Responsible for managing volumes
-Volumes are managed via VolumeReplicaMgr
-VolumeReplicaMgr is a collection of VolumeReplica(derives from PBMember)

##VolumeReplica
-Derives from PBReplica
-Overrides hooks onBecomeLeader(), onBecomeFollower()
Bootup
-Load the db
-Start PBMember bootup sequence
-If member is follower wait for leader to contact.  On message from leader check if sync is required and go through sync protocol.
-If member is leader as long as there are nonfunctional members keep sending group info messages so that nonfunctional members can become functional. 
###Applying writes
We journal the write first and then apply the write to database.  We do this because it is easy rollback a journal than db.  There is a possiblity for a crach between journal write and db commit. DB write updating the journal entry id along with update helps in detecting when journal and db are out of sync.  During restarts, we need to make sure journal and db are in sync.

#datom cli commands

datom addnode
>enter root
>discovered hardware
>service to run

datom startservice <svc>

datom stopservice <svc>
