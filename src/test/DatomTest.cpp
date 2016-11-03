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

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;
using namespace config;
using namespace volumeserver;

TEST(Datom, DISABLED_pbcluster)
{
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);
    bringupHelper.createPrimaryBackupDatasphere("datasphere1", 3);

    sleep(5);

    auto configService = bringupHelper.getConfigService();
    for (int i = 0; i < 3; i++) {
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

    testlib::waitForSIGINT();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
