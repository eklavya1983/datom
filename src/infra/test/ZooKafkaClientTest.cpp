#include <infra/ZooKafkaClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <folly/Format.h>
#include <folly/futures/Future.h>
#include <thread>
#include <infra/StatusException.h>

struct ZKHelper {
    ZKHelper()
    {
        start();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    ~ZKHelper()
    {
        cleanstop();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    void start() {
        auto cmd = folly::sformat("task startzk");
        int ret = std::system(cmd.c_str());
        ASSERT_TRUE(ret == 0);
    }
    void cleanstop() {
        auto cmd = folly::sformat("task cleanstopzk");
        int ret = std::system(cmd.c_str());
        ASSERT_TRUE(ret == 0);
    }
};

/* NOTE: Disabled because at the moment ZooKafkaClient::init will block until
 * zookeeper is available
 */
TEST(ZooKafkaClient, DISABLED_init_without_zk)
{
    infra::ZooKafkaClient client("test", "localhost:2181");
    ASSERT_THROW(client.init(), infra::StatusException);
}

TEST(ZooKafkaClient, basic_ops)
{
    ZKHelper h;
    infra::ZooKafkaClient client("test", "localhost:2181");
    ASSERT_NO_THROW(client.init());

    /* Ensure basic create get work */
    auto createResult = client.create("/keys", "keys");
    createResult.wait();
    ASSERT_FALSE(createResult.getTry().hasException());

    auto getResult = client.get("/keys");
    getResult.wait();
    ASSERT_FALSE(getResult.getTry().hasException());
    ASSERT_EQ(getResult.value().data, "keys");

    /* Add some children */
    auto res = client.create("/services", "")
        .then([&client]() {
                return client.create("/services/service1", "service1data");
              })
        .then([&client]() {
                return client.create("/services/service2", "service2data");
              });
    res.wait();
    ASSERT_FALSE(res.getTry().hasException());

    /* Ensure get children works */
    auto children = client.getChildrenSync("/services");
    ASSERT_EQ(children.size(), 2ull);
    auto itr = std::find_if(children.begin(), children.end(),
                 [](const infra::KVBinaryData &kvb) { return getId(kvb) == "service1";});
    ASSERT_TRUE(itr != children.end());
    itr = std::find_if(children.begin(), children.end(),
                       [](const infra::KVBinaryData &kvb) { return getId(kvb) == "service2";});
    ASSERT_TRUE(itr != children.end());

    /* Create children with watch test */
    int childWatchCntr = 0;
    auto watchFn = [&childWatchCntr](const std::string &path) {
        childWatchCntr++;
    };
    auto getChildrenRes  = client.getChildrenSimple("/services", watchFn);
    getChildrenRes.wait();
    ASSERT_EQ(getChildrenRes.get().size(), 2ull);
    ASSERT_EQ(childWatchCntr, 0);
    res = client.create("/services/service3", "service3data");
    res.wait();
    ASSERT_FALSE(res.getTry().hasException());
    ASSERT_EQ(childWatchCntr, 1);


    /* createIncludingAncestors test */
    res = client.createIncludingAncestors("/datom3/services/service1", "data");
    res.wait();
    ASSERT_FALSE(res.getTry().hasException());
    /* Create same path should fail as it's a duplicate */
    res = client.createIncludingAncestors("/datom3/services/service1", "data");
    res.wait();
    ASSERT_TRUE(res.getTry().hasException());
    /* Create an addtional path with ancestry sharing should succeed */
    res = client.createIncludingAncestors("/datom3/services/service2/service3", "data");
    res.wait();
    ASSERT_FALSE(res.getTry().hasException());

    /* set test */
    /* set on non-existent node should fail */
    auto versionRes = client.set("/datom4", "data", -1);
    versionRes.wait();
    ASSERT_TRUE(versionRes.getTry().hasException());
    /* set on existing node should succeed */
    versionRes = client
        .create("/datom4", "data")
        .then([&client]() {
              return client.set("/datom4", "datom4", -1);
          });
    versionRes.wait();
    ASSERT_FALSE(versionRes.getTry().hasException());
    ASSERT_EQ(client.get("/datom4").get().data, "datom4");

    /* delete test */
    /* deleting existing key should succeed */
    auto delRes = client.del("/datom4", -1);
    delRes.wait();
    ASSERT_FALSE(delRes.getTry().hasException());
    /* get on deleted key should fail */
    getResult = client.get("/datom4");
    getResult.wait();
    ASSERT_TRUE(getResult.getTry().hasException());

}


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
