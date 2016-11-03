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

TEST(ServiceTest, DISABLED_init)
{
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);

    bringupHelper.addDataSphere("sphere1");

    ServiceInfo serviceInfo;
    serviceInfo.dataSphereId = "sphere1";
    serviceInfo.nodeId = "node1";
    serviceInfo.id = "service1";

    std::unique_ptr<Service> service1 (Service::newDefaultService(serviceInfo.id,
                                                                  serviceInfo,
                                                                  "localhost:2181"));
    /* Init without service being added to datasphere should fail */
    ASSERT_THROW(service1->init(), StatusException);

    bringupHelper.addService("sphere1", "node1", "service1", "127.0.0.1", 8080);
    service1.reset(Service::newDefaultService(serviceInfo.id,
                                              serviceInfo,
                                              "localhost:2181"));
    service1->init();

    /* Add another service */
    serviceInfo.id = "service2";
    bringupHelper.addService("sphere1", "node1", serviceInfo.id, "127.0.0.1", 8084);
    std::unique_ptr<Service> service2 (Service::newDefaultService(serviceInfo.id,
                                                                  serviceInfo,
                                                                  "localhost:2181"));
    service2->init();


    // testlib::waitForSIGINT();
}

class MySocket : public TAsyncSocket {
 public:
  typedef std::unique_ptr<MySocket, Destructor> UniquePtr;
    MySocket(folly::EventBase* evb,
        const std::string& ip,
        uint16_t port,
        uint32_t connectTimeout = 0)
        : TAsyncSocket(evb, ip, port, connectTimeout)
    {
    }
#if 0
    using TAsyncSocket::TAsyncSocket;

    static std::shared_ptr<MySocket> newSocket(
        folly::EventBase* evb,
        const std::string& ip,
        uint16_t port,
        uint32_t connectTimeout = 0) {
        return std::shared_ptr<MySocket>(
            new MySocket(evb, ip, port, connectTimeout),
            folly::DelayedDestruction::Destructor());
    }
    void reconnect() {
        eventBase_->runInEventBaseThread([this]() {
            close();
            init();
            connect(nullptr, "127.0.0.1", 8082, 5000);
        });
    }
    void doReconnect(TAsyncSocket &sock) {
        sock.getEventBase()->runInEventBaseThread([this, &sock]() {
            sock.close();
            init();
        });
    }
#endif
    
};

TEST(Service, testserver)
{
    infra::ServiceInfo info;
    info.id = "service1";
    info.ip = "localhost";
    info.port = 8082;
    Service *s = new Service("test", info, std::make_shared<ServiceApiHandler>(), nullptr);
    s->init();
    std::thread t([s]() { s->run(); });
    std::this_thread::sleep_for(std::chrono::seconds(2));

    folly::EventBase base;
    auto interval = base.timer().getTickInterval();
    interval++;
    std::shared_ptr<MySocket> socket(new MySocket(&base, "127.0.0.1", 8082, 5000),
                                     folly::DelayedDestruction::Destructor());
        /*
    std::shared_ptr<MySocket> socket(
        MySocket::newSocket(&base, "127.0.0.1", 8082, 5000));
        */

    infra::ServiceApiAsyncClient client(
        std::unique_ptr<HeaderClientChannel,
        folly::DelayedDestruction::Destructor>(
            new HeaderClientChannel(socket)));

    boost::polymorphic_downcast<HeaderClientChannel*>(
        client.getChannel())->setTimeout(5000);
    std::string response;
    auto f = client.future_getModuleState({});
    base.loop();
    ASSERT_TRUE(f.get() ==  "ok");

    client.sync_getModuleState(response, {});
    EXPECT_EQ(response, "ok");
    LOG(INFO) << "received responses from service1.  sock good?:" << socket->good();

    s->shutdown();
    t.join();
    delete s;
    LOG(INFO) << "Killed service1  sock good?:" << socket->good();

    for (int i = 0; i < 5; i++) {
        LOG(INFO) << "future_getModuleState attempt: " << i << " sock good?:" << socket->good();
        f = client.future_getModuleState({});
        base.loop();
        f.wait();
        ASSERT_TRUE(f.hasException());
    }

    LOG(INFO) << "Spawn service2 sock good?:" << socket->good();
    s = new Service("test", info, std::make_shared<ServiceApiHandler>(), nullptr);
    s->init();
    std::thread t2([s]() { s->run(); });
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // socket->reconnect();
    // base.loop();

    for (int i = 0; i < 5; i++) {
        LOG(INFO) << "future_getModuleState attempt: " << i << " sock good?:" << socket->good();
        f = client.future_getModuleState({});
        base.loop();
        if (f.hasException()) {
            LOG(INFO) << "has excpetion: " << f.getTry().exception().what();
        } else {
            ASSERT_TRUE(f.get() ==  "ok");
        }
    }
    s->shutdown();
    t2.join();
#if 0
    client.sync_getModuleState(response, {});
    EXPECT_EQ(response, "ok");

    RpcOptions options;
    options.setTimeout(std::chrono::milliseconds(1));
    try {
        // should timeout
        client.sync_sendResponse(options, response, 10000);
    } catch (const TTransportException& e) {
        EXPECT_EQ(int(TTransportException::TIMED_OUT), int(e.getType()));
        return;
    }
    ADD_FAILURE();
#endif

  delete s;
}

#if 0
struct Handler : infra::cpp2::ServiceApiSvIf 
{
  void getModuleState(std::string& _return, std::unique_ptr<std::map<std::string, std::string>> arguments) override {
      _return = "hello";
      LOG(INFO) << "returning hello";
  }
};

TEST(Service, init)
{
    //     infra::Service<> s("Service", "data", "localhost", 8080, "");
    // s.init();
    std::thread t([]() {
                  apache::thrift::ThriftServer* server_ = new apache::thrift::ThriftServer("disabled", false);
                  server_->setPort(8082);
                  server_->setInterface(std::make_shared<Handler>());
                  // server_->setIOThreadPool(system_->getIOThreadPool());
                  server_->setNWorkerThreads(2);
                  server_->serve();
    });
    std::this_thread::sleep_for(std::chrono::seconds(2));

#if 0
    using namespace apache::thrift::transport;
    using apache::thrift::protocol::TBinaryProtocolT;
    std::shared_ptr<TSocket> socket = std::make_shared<TSocket>("localhost", 8082);
    socket->open();

    std::shared_ptr<TFramedTransport> transport =
        std::make_shared<TFramedTransport>(socket);
    std::shared_ptr<TBinaryProtocolT<TBufferBase>> protocol =
        std::make_shared<TBinaryProtocolT<TBufferBase>>(transport);
    auto client = std::make_shared<infra::ServiceApiClient>(protocol);
    std::string ret;
    client->getModuleState(ret, {});
#endif

    t.join();
}
#endif


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
