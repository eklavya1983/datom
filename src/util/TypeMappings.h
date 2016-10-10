#pragma once
#include <actor/ActorMsg.h>


namespace bhoomi {
using namespace actor;
using namespace cpp2;

void initActorMsgMappings() {
    actor::initActorSystemMappings();
    ADD_MSGMAPPING(GroupAddVolume,                 9);
    ADD_MSGMAPPING(GroupPutObject,                 10);
    ADD_MSGMAPPING(GroupPutObjectResp,             11);
    ADD_MSGMAPPING(GroupGetObject,                 12);
    ADD_MSGMAPPING(GroupGetObjectResp,             13);
}
}  // namespace bhoomi
