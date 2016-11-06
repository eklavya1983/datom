include "commontypes.thrift"

namespace cpp infra

service ServiceApi {
	string getModuleState(1: map<string, string> arguments)
	commontypes.KVBinaryData handleKVBMessage(1: commontypes.KVBinaryData message)
}

struct PingMsg {
}

struct PingRespMsg {
}
