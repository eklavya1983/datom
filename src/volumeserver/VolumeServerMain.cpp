#include <infra/InfraForwards.h>
#include <volumeserver/VolumeServer.h>
#include <infra/ZooKafkaClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>
#include <gflags/gflags.h>
#include <testlib/SignalUtils.h>

DEFINE_string(datasphere, "datasphere1", "datasphere id");
DEFINE_string(node, "node1", "node id");
DEFINE_string(service, "volumeserver1", "service id");
DEFINE_string(ip, "localhost", "ip");
DEFINE_int32(port, 2085, "port");

using namespace infra;
using namespace volume;

int main() {
    ServiceInfo info;
    info.dataSphereId = FLAGS_datasphere;
    info.nodeId = FLAGS_node;
    info.id = FLAGS_service;
    info.type = ServiceType::VOLUME_SERVER;
    info.ip = FLAGS_ip;
    info.port = FLAGS_port;

    auto configClient = std::make_shared<ZooKafkaClient>(info.id,
                                                         "localhost:2181/datom",
                                                         info.id);
    
    auto service = std::make_shared<VolumeServer>(info.id,
                                                  info,
                                                  std::make_shared<ServiceApiHandler>(),
                                                  configClient);
    service->init();
    testlib::waitForSIGINT();
    return 0;
}
