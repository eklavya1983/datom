namespace cpp infra 

const string PB_LOCK_KEY="LOCK"

struct GetMemberStateMsg {
    1: string		groupType;
    2: i64		groupId;
}

struct GetMemberStateRespMsg {
    1: i64 		commitId;
}
