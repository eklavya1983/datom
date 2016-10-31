include "commontypes.thrift"

namespace cpp infra

service ConfigApi {
	void addDatasphere(1: commontypes.DataSphereInfo inf);
	void addService(1: commontypes.ServiceInfo info);
	void addVolume(1: commontypes.VolumeInfo info);

	list<commontypes.ServiceInfo> listServices(1: string datasphere);
	list<commontypes.VolumeInfo> listVolumes(1: string datasphere);
	list<commontypes.RingInfo> listVolumeRings(1: string datasphere);
}
