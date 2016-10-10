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

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;
using namespace config;

TEST(ConfigServiceTest, basic)
{
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);

    DataSphereInfo datasphere;
    datasphere.id = "sphere1";

    ServiceInfo serviceInfo;
    serviceInfo.dataSphereId = "sphere1";
    serviceInfo.nodeId = "node1";
    serviceInfo.id = "service1";
    serviceInfo.type = ServiceType::VOLUME_SERVER;
    serviceInfo.ip = "localhost";
    serviceInfo.port = 2181;

    auto configService = bringupHelper.getConfigService();
    configService->addDataSphere(datasphere);
    configService->addService(serviceInfo);

    serviceInfo.id = "service2";
    configService->addService(serviceInfo);

    serviceInfo.id = "service3";
    configService->addService(serviceInfo);

    /* Create a volume */
    VolumeInfo vol;
    vol.datasphereId = "sphere1";
    vol.name = "vol1";
    auto retVol = configService->addVolume(vol);
    ASSERT_EQ(retVol.id, 0);

    testlib::waitForSIGINT();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
