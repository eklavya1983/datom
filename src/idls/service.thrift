include "commontypes.thrift"

namespace cpp infra

/* File system root paths */
const string FS_VOLUMES_PATH="{{}}/volumes"
const string FS_DATA_PATH="{{}}/data"

service ServiceApi {
	string getModuleState(1: map<string, string> arguments)
	commontypes.KVBuffer handleKVBMessage(1: commontypes.KVBuffer message)
}
