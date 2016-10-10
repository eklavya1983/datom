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

using namespace apache::thrift::async;
using namespace apache::thrift;
using namespace infra;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
