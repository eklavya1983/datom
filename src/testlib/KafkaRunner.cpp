#include <testlib/KafkaRunner.h>
#include <thread>
#include <gtest/gtest.h>
#include <folly/Format.h>

DEFINE_string(toolsdir, "", "tools directory");

namespace testlib {

KafkaRunner::KafkaRunner()
{
    if (FLAGS_toolsdir.empty()) {
        throw std::runtime_error("toolsdir flag isn't specified");
    }
    taskScript_ = folly::sformat("{}/task.sh", FLAGS_toolsdir);

}

void KafkaRunner::cleanstart() {
    auto cmd = folly::sformat("{} cleanstartdatom", taskScript_);
    int ret = std::system(cmd.c_str());
    ASSERT_TRUE(ret == 0);
}

void KafkaRunner::cleanstop() {
    auto cmd = folly::sformat("{} cleanstopdatom", taskScript_);
    int ret = std::system(cmd.c_str());
    ASSERT_TRUE(ret == 0);
}

void KafkaRunner::stop() {
    auto cmd = folly::sformat("{} stopdatom", taskScript_);
    int ret = std::system(cmd.c_str());
    ASSERT_TRUE(ret == 0);
}

ScopedKafkaRunner::ScopedKafkaRunner()
{
    runner_.cleanstart();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

ScopedKafkaRunner::~ScopedKafkaRunner()
{
    runner_.cleanstop();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

}  // namespace testlib
