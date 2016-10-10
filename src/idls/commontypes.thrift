namespace cpp infra 
namespace java infra.gen

struct DataSphereInfo {
	1: string 			id;
}

struct NodeInfo {
	1: string 			id;
	2: string 			dataSphereId;
	3: list<string> 		serviceIds;
}

enum ServiceType {
	VOLUME_SERVER = 1,
	CLI = 2
}

struct ServiceInfo {
	1: string 			id;
	2: string 			dataSphereId;
	3: string 			nodeId;
	4: ServiceType			type;
	5: string 			ip;
	6: i32				port;
}

struct VolumeInfo {
	1: i64				id;
	2: string			name;
	3: string			datasphereId;
	4: i32				ringId;
}

struct RingInfo {
	1: i32				id;
	2: list<string>			memberIds;
}

const i32 INVALID_VERSION = -1;

/* Common keys */
const string KEY_VERSION                        = "version"
const string KEY_TYPE                           = "type"
const string KEY_ID				= "id"

/* Holds binary data with some properties.  Typical properties include type, version, etc. */
struct KVBinaryData {
	1: map<string, string>  props;
	2: binary               data;
}

struct Properties {
	1: map<string, string>		props;
}
