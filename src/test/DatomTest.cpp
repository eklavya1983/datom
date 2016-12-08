#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <folly/Format.h>
#include <folly/futures/Future.h>
#include <thread>
#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
#include <infra/Service.h>
#include <infra/gen/gen-cpp2/ServiceApi.h>
#include <infra/CoordinationClient.h>
#include <infra/StatusException.h>
#include <boost/cast.hpp>
#include <testlib/DatomBringupHelper.tcc>
#include <testlib/SignalUtils.h>
#include <infra/ZooKafkaClient.h>
#include <configservice/ConfigService.h>
#include <volumeserver/VolumeServer.h>
#include <infra/MessageUtils.tcc>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;
using namespace config;
using namespace volume;

TEST(Datom, pbcluster)
{
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);
    DataSphereInfo datasphere;
    datasphere.id = "datasphere1";
    auto configService = bringupHelper.getConfigService();
    configService->addDataSphere(datasphere);

    std::vector<ServiceInfo> serviceInfos;
    for (int i = 0; i < 3; i++) {
        auto serviceInfo = bringupHelper.generateVolumeServiceInfo(datasphere.id, i);
        configService->addService(serviceInfo);
        serviceInfos.push_back(serviceInfo);
        NodeRoot(serviceInfo.rootPath).makeNodeRootTree();
    }

    auto configClient = std::make_shared<ZooKafkaClient>(serviceInfos[0].id,
                                                         "localhost:2181/datom",
                                                         serviceInfos[0].id);

    auto service = std::make_shared<VolumeServer>(serviceInfos[0].id,
                                                  serviceInfos[0],
                                                  std::make_shared<ServiceApiHandler>(),
                                                  configClient);
    service->init();
    service->run(true);

    auto configClient2 = std::make_shared<ZooKafkaClient>(serviceInfos[1].id,
                                                         "localhost:2181/datom",
                                                         serviceInfos[1].id);

    auto service2 = std::make_shared<VolumeServer>(serviceInfos[1].id,
                                                  serviceInfos[1],
                                                  std::make_shared<ServiceApiHandler>(),
                                                  configClient2);
    service2->init();
    service2->run(true);

#if 0
    auto f = sendKVBMessage<PingMsg, PingRespMsg>(service->getConnectionCache(),
                                                  serviceInfos[1].id,
                                                  PingMsg());
    f.wait();
    ASSERT_TRUE(f.hasException());
#endif

    sleep(5);

    for (int i = 0; i < 1; i++) {
        VolumeInfo vol;
        vol.datasphereId = "datasphere1";
        vol.name = folly::sformat("vol{}", i);
        auto retVol = configService->addVolume(vol);
        ASSERT_EQ(retVol.id, i);
    }

#if 0
    DataSphereInfo datasphere;
    datasphere.id = "sphere1";
    auto configService = bringupHelper.getConfigService();
    configService->addDataSphere(datasphere);

    ServiceInfo serviceInfo1;
    serviceInfo1.dataSphereId = "sphere1";
    serviceInfo1.nodeId = "node1";
    serviceInfo1.id = "service1";
    serviceInfo1.type = ServiceType::VOLUME_SERVER;
    serviceInfo1.ip = "localhost";
    serviceInfo1.port = 2181;

    configService->addService(serviceInfo1);

    auto serviceInfo2 = serviceInfo1;
    serviceInfo2.id = "service2";
    configService->addService(serviceInfo2);

    auto serviceInfo3 = serviceInfo1;
    serviceInfo3.id = "service3";
    configService->addService(serviceInfo3);

    /* Create a volume */
    VolumeInfo vol;
    vol.datasphereId = "sphere1";
    vol.name = "vol1";
    auto retVol = configService->addVolume(vol);
    ASSERT_EQ(retVol.id, 0);

    VolumeServer service1(serviceInfo1.id,
                          serviceInfo1,
                          false,
                          std::make_shared<ZooKafkaClient>(serviceInfo1.id,
                                                           "localhost:2181/datom",
                                                           serviceInfo1.id));
    service1.init();
#endif
    testlib::waitForKeyPress();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
