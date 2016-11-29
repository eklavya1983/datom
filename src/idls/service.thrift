include "commontypes.thrift"

namespace cpp infra

service ServiceApi {
	string getModuleState(1: map<string, string> arguments)
	commontypes.KVBuffer handleKVBMessage(1: commontypes.KVBuffer message)
}
