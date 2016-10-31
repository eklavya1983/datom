#include <infra/InfraForwards.h>
#include <configservice/ConfigService.h>
#include <infra/ZooKafkaClient.h>
#include <gflags/gflags.h>
#include <testlib/SignalUtils.h>

using namespace infra;
using namespace config;

int main() {
    std::string id = "ConfigService";
    auto zkClient = std::make_shared<ZooKafkaClient>(id,
                                                     "localhost:2181/datom",
                                                     id);
    
    auto serviceInfo = ServiceInfo();
    serviceInfo.port = 9090;
    serviceInfo.ip = "localhost";
    auto configService = std::make_shared<ConfigService>(id,
                                                         serviceInfo,
                                                         zkClient);
    configService->init();

    /* Create datom namespace */
    configService->createDatom();

    configService->run();
    return 0;
}
