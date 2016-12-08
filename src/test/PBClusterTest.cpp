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
#include <infra/MessageUtils.tcc>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>
#include <infra/PBMember.h>

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;
using namespace config;
using namespace volume;

struct FakeService : Service {
    using Service::Service;
};

std::shared_ptr<FakeService> newFakeService(
    testlib::DatomBringupHelper<ConfigService> &bringupHelper,
    const ServiceInfo &info)
{
    bringupHelper.getConfigService()->addService(info);

    auto configClient = std::make_shared<ZooKafkaClient>(info.id,
                                                         "localhost:2181/datom",
                                                         info.id);

    auto service = std::make_shared<FakeService>(info.id,
                                                 info,
                                                 std::make_shared<ServiceApiHandler>(),
                                                 configClient);
    return service;
}

TEST(pbcluster, groupwrite) {
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);
    DataSphereInfo datasphere;
    datasphere.id = "datasphere1";
    auto configService = bringupHelper.getConfigService();
    configService->addDataSphere(datasphere);

    std::vector<ServiceInfo> infos;
    for (uint32_t i = 0; i < 3; i++) {
        infos.push_back(bringupHelper.generateServiceInfo("datasphere1", i, ServiceType::TEST));
    }
    auto service1 = newFakeService(bringupHelper, infos[0]);
    service1->init();
    service1->run(true);
    testlib::waitForKeyPress();

    PBMember leader(service1->getLogContext(),
                    service1->getEventBaseFromPool(),
                    service1.get(),
                    0 /* resourceId */,
                    "" /* groupKey */,
                    { infos[1].id, infos[2].id } /* members */,
                    infos[0].id,
                    2 /* quorum */);

}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
