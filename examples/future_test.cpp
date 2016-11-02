#include <iostream>
#include <folly/futures/Future.h>
#include <folly/futures/Future-inl.h>
#include <folly/io/async/EventBase.h>

#include <glog/logging.h>
#include <gtest/gtest.h>


using namespace folly;
using namespace std;
int getInt();

thread_local int tInt = getInt();

int getInt() {
    return tInt;
}

folly::Future<Unit> makeEventbaseWork(folly::EventBase *eb,
                                      const std::function<void()> &work)
{
    return makeFuture().via(eb).then([work]() {work();});
}

TEST(thread, create) {
    tInt = 10;
    std::thread t([]() {
                  std::cout << "tInt: " << tInt;
                  });
    /*
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    */
    t.join();
}

TEST(Future, basic) {
    Promise<int> p;
    Future<int> f = p.getFuture();
    f
    .then([](int x) {
          cout << "lamda x: " << x << endl;
          throw std::runtime_error("error from lamda1");
    })
    .then([]() {
          cout << "lamda 2 " << endl;
    })
    .onError([](std::exception e) {
             cout << "exceptin: " << e.what();
     });
    cout << "Future chain made" << endl;

    cout << "fulfilling Promise" << endl;
    p.setValue(42);
    cout << "Promise fulfilled" << endl;
}

TEST(Future, noRef) {
    folly::EventBase eb;
    std::thread t([&eb]() {eb.loopForever();});
    folly::EventBase eb2;
    std::thread t2([&eb2]() {eb2.loopForever();});
    auto someWork = [&eb, &eb2]() {
        auto f =
            folly::makeFuture()
            .via(&eb)
            .then([&eb, &eb2]() {
                  CHECK(eb.isInEventBaseThread());
                  std::cout << "first function\n";
                  return makeEventbaseWork(&eb2,
                                           [&eb2]() {
                                               CHECK(eb2.isInEventBaseThread());
                                               std::cout << "eb2 work1\n";
                                           });
            })
            .then([&eb, &eb2]() {
                  CHECK(eb.isInEventBaseThread());
                  return makeEventbaseWork(&eb2,
                                           [&eb2]() {
                                               CHECK(eb2.isInEventBaseThread());
                                               std::cout << "eb2 work2\n";
                                           });
            })
            .then([&eb, &eb2]() {
                  CHECK(eb.isInEventBaseThread());
                  std::cout << "second function\n";
            });
    };

    someWork();

    sleep(2);
    eb.terminateLoopSoon();
    eb2.terminateLoopSoon();
    t.join();
    t2.join();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}


