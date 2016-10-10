include "commontypes.thrift"

namespace cpp infra

service ConfigApi {
	void addService(1: commontypes.ServiceInfo info);
	void addVolume(1: commontypes.VolumeInfo info);
}
