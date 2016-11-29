#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <folly/Format.h>
#include <folly/futures/Future.h>
#include <thread>
#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>
#include <infra/Service.h>
#include <infra/gen/gen-cpp2/ServiceApi.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>
#include <infra/CoordinationClient.h>
#include <infra/ConnectionCache.tcc>
#include <infra/StatusException.h>
#include <boost/cast.hpp>
#include <testlib/DatomBringupHelper.tcc>
#include <testlib/SignalUtils.h>
#include <infra/ZooKafkaClient.h>
#include <configservice/ConfigService.h>
#include <volumeserver/VolumeServer.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <infra/gen/gen-cpp2/service_types.tcc>
#include <infra/MessageUtils.tcc>

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;
using namespace config;
using namespace volume;

#define SUBTEST(_subtest_, _lamda_) \
    TLog << __subtest_; \
    _lamda_();

struct FakeService : Service
{
    FakeService(const ServiceInfo &info)
        : Service(info.id,
                  info,
                  std::make_shared<ServiceApiHandler>(),
                  std::make_shared<ZooKafkaClient>(info.id, "localhost:2181/datom", info.id))
    {}
    virtual void init() {
        Service::init();
        initIOThreadpool_(2);
    }
};

TEST(ServiceTest, connection_up_down) {
    testlib::DatomBringupHelper<ConfigService> bringupHelper;
    testlib::ScopedDatom<ConfigService> d(bringupHelper);
    bringupHelper.addDataSphere("sphere1");

    /* Create service1 */
    auto serviceInfo1 = bringupHelper.generateVolumeServiceInfo("sphere1", 1);
    bringupHelper.getConfigService()->addService(serviceInfo1);
    std::unique_ptr<Service> service1 (new FakeService(serviceInfo1));
    service1->init();
    auto connCache1 = service1->getConnectionCache();

    TLog << "Try and send a message to service2 which isn't up yet.  It should fail";
    auto f = connCache1
        ->getAsyncClient<infra::ServiceApiAsyncClient>("service2");
    f.wait();
    ASSERT_TRUE(f.hasException());

    TLog << "Try sendKVBMessage against service2 which isn't up yet.  It should fail";
    auto sendF = sendKVBMessage<PingMsg, PingRespMsg>(connCache1, "service2", PingMsg());
    sendF.wait();
    ASSERT_TRUE(sendF.hasException());

    TLog << "Bring up service2 .  Send a message to service2 now and it should work";
    auto serviceInfo2 = bringupHelper.generateVolumeServiceInfo("sphere1", 2);
    bringupHelper.getConfigService()->addService(serviceInfo2);
    std::unique_ptr<Service> service2 (new FakeService(serviceInfo2));
    service2->init();
    service2->run(true /*async*/);
    sleep(2);

    for (int i = 0; i < 2; i++) {
        f = connCache1
            ->getAsyncClient<infra::ServiceApiAsyncClient>("service2");
        f.wait();
        ASSERT_TRUE(!f.hasException());
        auto svc2Client = f.get();
        auto moduleStateFut = svc2Client->future_getModuleState({});
        moduleStateFut.wait();
        ASSERT_TRUE(connCache1->\
                    getHeaderClientChannelFromCache("service2").get().get() != nullptr);
        ASSERT_TRUE(!moduleStateFut.hasException()) << moduleStateFut.getTry().exception().what();
    }

    TLog << "Bring down service2.  Sending a message should fail";
    service2.reset();
    f = connCache1
        ->getAsyncClient<infra::ServiceApiAsyncClient>("service2");
    f.wait();
    ASSERT_TRUE(!f.hasException());
    auto svc2Client = f.get();
    auto moduleStateFut = svc2Client->future_getModuleState({});
    moduleStateFut.wait();
    ASSERT_TRUE(moduleStateFut.hasException());

    TLog << "Bring up service2.  Sending a message should succeed";
    service2.reset(new FakeService(serviceInfo2));
    service2->init();
    service2->run(true /*async*/);
    sleep(2);

    for (int i = 0; i < 2; i++) {
        f = connCache1
            ->getAsyncClient<infra::ServiceApiAsyncClient>("service2");
        f.wait();
        ASSERT_TRUE(!f.hasException());
        auto svc2Client = f.get();
        auto moduleStateFut = svc2Client->future_getModuleState({});
        moduleStateFut.wait();
        ASSERT_TRUE(connCache1->\
                    getHeaderClientChannelFromCache("service2").get().get() != nullptr);
        ASSERT_TRUE(!moduleStateFut.hasException()) << moduleStateFut.getTry().exception().what();
    }

    TLog << "Two clients sending messages to same service simulataneously";
    svc2Client = connCache1->\
                 getAsyncClient<infra::ServiceApiAsyncClient>("service2").\
                 get();
    auto svc2Client2 = connCache1->\
                 getAsyncClient<infra::ServiceApiAsyncClient>("service2").\
                 get();
    auto task = [](const std::shared_ptr<infra::ServiceApiAsyncClient> client) {
        for (int i = 0; i < 100; i++) {
            auto f = client->future_getModuleState({{"k1", "v1"}, {"k2", "v2"}});
            f.wait();
            ASSERT_TRUE(!f.hasException());
        }
    };
    std::thread t1([=]() {
        task(svc2Client);
    });
    std::thread t2([=]() {
        task(svc2Client2);
    });
    t1.join();
    t2.join();

    TLog << "Test handling valid handleKVBMessage";
    auto handler2 = service2->getHandler<ServiceApiHandler>();
    handler2->registerKVBMessageHandler(
        "PingMsg",
        [](std::unique_ptr<KVBinaryData> kvb) {
            std::string req = getProp<std::string>(*kvb, "request");
            if (req == "ping") {
                setProp<std::string>(*kvb, "response", "pong");
            }
            return folly::makeFuture(std::move(kvb));
        });
    svc2Client = connCache1->\
                 getAsyncClient<infra::ServiceApiAsyncClient>("service2").\
                 get();

    KVBinaryData reqKvb;
    setType(reqKvb, "PingMsg");
    setProp(reqKvb, "request", "ping");
    auto respF = svc2Client->future_handleKVBMessage(reqKvb);
    auto respKvb = respF.get();
    ASSERT_EQ(getProp<std::string>(respKvb, "response"), "pong");

    TLog << "Test handling invallid handleKVBMessage";
    setType(reqKvb, "InvalidType");
    respF = svc2Client->future_handleKVBMessage(reqKvb);
    respF.wait();
    ASSERT_TRUE(respF.hasException());

    TLog << "Test handling invallid handleKVBMessage";
    handler2->registerKVBMessageHandler(
        "PingMsg",
        KVBThriftJsonHandler<PingMsg, PingRespMsg>(
            [](std::unique_ptr<PingMsg> req) {
                return folly::makeFuture(folly::make_unique<PingRespMsg>());
            }));
    setType(reqKvb, "PingMsg");
    reqKvb.data = serializeToThriftJson<PingMsg>(PingMsg(), "");
    respF = svc2Client->future_handleKVBMessage(reqKvb);
    respKvb = respF.get();
    deserializeThriftJsonData<PingRespMsg>(respKvb,"");

    testlib::waitForKeyPress();
    service1->shutdown();
    service2->shutdown();


#if 0
    folly::EventBase base;
    std::shared_ptr<ata::TAsyncSocket> socket(new ata::TAsyncSocket(&base,
                                                                    "127.0.0.1", 2105, 5000),
                                     folly::DelayedDestruction::Destructor());
    infra::ServiceApiAsyncClient client(
        std::unique_ptr<HeaderClientChannel,
        folly::DelayedDestruction::Destructor>(
            new HeaderClientChannel(socket)));

    boost::polymorphic_downcast<HeaderClientChannel*>(
        client.getChannel())->setTimeout(5000);
    std::string response;
    auto f2 = client.future_getModuleState({});
    base.loop();
    ASSERT_TRUE(f2.get() ==  "ok");
#endif

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
                                                                  "localhost:2181/datom"));
    /* Init without service being added to datasphere should fail */
    ASSERT_THROW(service1->init(), StatusException);

    bringupHelper.addService("sphere1", "node1", "service1", "127.0.0.1", 8080);
    service1.reset(Service::newDefaultService(serviceInfo.id,
                                              serviceInfo,
                                              "localhost:2181/datom"));
    service1->init();

    /* Add another service */
    serviceInfo.id = "service2";
    bringupHelper.addService("sphere1", "node1", serviceInfo.id, "127.0.0.1", 8084);
    std::unique_ptr<Service> service2 (Service::newDefaultService(serviceInfo.id,
                                                                  serviceInfo,
                                                                  "localhost:2181/datom"));
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

TEST(Service, DISABLED_testserver)
{
    infra::ServiceInfo info;
    info.id = "service1";
    info.ip = "127.0.0.1";
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

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
