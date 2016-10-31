#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp/concurrency/PosixThreadFactory.h>
#include <thrift/lib/cpp/util/ScopedServerThread.h>
#include <util/Log.h>
#include <infra/ServiceServer.h>
#include <infra/gen/gen-cpp2/ServiceApi.h>

namespace infra {

ServiceServer::ServiceServer(const std::string &logContext,
                             const std::string &ip,
                             int port,
                             const std::shared_ptr<ServerHandler> &handler)
{
    logContext_ = logContext;
    ip_ = ip;
    port_ = port;
#if 0
    if (handler == nullptr) {
        handler = std::make_shared<cpp2::ServiceApiHandler>(new ServiceApiHandler());
    }
#endif
    server_.reset(new apache::thrift::ThriftServer("disabled", false));
    server_->setPort(port_);
    server_->setInterface(handler);
}

void ServiceServer::start() {
#if 0
    if (true) {
        std::shared_ptr<apache::thrift::concurrency::ThreadFactory> threadFactory(
            new apache::thrift::concurrency::PosixThreadFactory);
        std::shared_ptr<apache::thrift::concurrency::ThreadManager> threadManager(
            apache::thrift::concurrency::ThreadManager::newSimpleThreadManager(
                1, 1000, false, 0));
        threadManager->threadFactory(threadFactory);
        threadManager->start();
        server_->setThreadManager(threadManager);
    }

    if (block) {
        server_->serve();
    } else {
        serverThread_ = new apache::thrift::util::ScopedServerThread(server_);
    }
#endif
    CLog(INFO) << "Starting server at port:" << port_;
    // TODO(Rao): Configure the server
    // server_->setIOThreadPool(system_->getIOThreadPool());
    server_->setNWorkerThreads(2);

    std::shared_ptr<apache::thrift::concurrency::PosixThreadFactory> threadFactory(
        new apache::thrift::concurrency::PosixThreadFactory);
    std::shared_ptr<apache::thrift::concurrency::ThreadManager> threadManager =
      apache::thrift::concurrency::ThreadManager::newSimpleThreadManager(2);
    threadManager->threadFactory(threadFactory);
    threadManager->start();

    server_->setThreadManager(threadManager);
    // server_->setNPoolThreads(2);
    server_->serve();
}

void ServiceServer::stop() {
    server_->stop();
    if (serverThread_) {
        delete serverThread_;
        serverThread_ = nullptr;
    }
}

}  // namespace actor
