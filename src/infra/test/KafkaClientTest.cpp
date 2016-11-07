#include <infra/KafkaClient.h>
#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <librdkafka/rdkafkacpp.h>
#include <util/Log.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <testlib/KafkaRunner.h>

DEFINE_string(group, "service1", "consumer service group");

#if 0
DEFINE_string(toolsdir, "~/playground/cpp/test_fbthrift/src/tools", "tools directory");
struct KafkaRunner {
    KafkaRunner()
    {
        start();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    ~KafkaRunner()
    {
        stop();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    void start() {
        auto cmd = folly::sformat("{}/task.sh cleanstartdatom", FLAGS_toolsdir);
        int ret = std::system(cmd.c_str());
        ASSERT_TRUE(ret == 0);
    }
    void stop() {
        auto cmd = folly::sformat("{}/task.sh cleanstopdatom", FLAGS_toolsdir);
        int ret = std::system(cmd.c_str());
        ASSERT_TRUE(ret == 0);
    }
};
#endif


TEST(KafkaClient, init)
{
    testlib::ScopedKafkaRunner kafkaRunner;

    infra::KafkaClient client(FLAGS_group, "localhost", FLAGS_group);
    client.init();

    /* Create topic first.  Empty message to publishMessage means create topic */
    auto status = client.publishMessage("test", std::string());
    ASSERT_TRUE(status == infra::Status::STATUS_OK);

    int recvCnt = 0;
    client.subscribeToTopic("test",
                            [&recvCnt](int64_t seq, const std::string &payload) {
                                LOG(INFO) << "recv seq:" << seq << " msg:" << payload;
                                recvCnt++;
                            });

    /* This sleep is necessary because based on some tests, it takes a little
     * bit of time for subscription to register.  We won't receive below
     * published messages if subscription hasn't registered.  see DAT-23
     */
    std::this_thread::sleep_for(std::chrono::seconds(3));

    int sentCnt = 0;
    for (; sentCnt < 10; sentCnt++) {
        auto status = client.publishMessage("test", "hello");
        ASSERT_TRUE(status == infra::Status::STATUS_OK);
    }

    int sleptSecs = 0;
    while (sentCnt != recvCnt && sleptSecs < 32) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        sleptSecs++;
    }
    ASSERT_EQ(recvCnt, sentCnt);
}


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
