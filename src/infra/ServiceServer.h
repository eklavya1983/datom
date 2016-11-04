#pragma once
#include <memory>

namespace apache {
namespace thrift {
class ServerInterface;
class ThriftServer;
namespace util {
class ScopedServerThread;
}  // namespace util
}  // namespace thrift
}  // namespace apache

namespace infra {

using ServerHandler = apache::thrift::ServerInterface;
/**
 * @brief Default server for service
 */
struct ServiceServer {
    ServiceServer(const std::string &logContext,
                  const std::string &ip,
                  int port,
                  const std::shared_ptr<ServerHandler> &handler);
    virtual ~ServiceServer() = default;
    ServiceServer(const ServiceServer&) = delete;
    void operator=(ServiceServer const &) = delete;

    virtual void start();
    virtual void stop();

    std::shared_ptr<ServerHandler> getHandler() const { return handler_; }

    const std::string& getLogContext() const { return logContext_; }

 protected:
    std::string                                             logContext_;
    std::string                                             ip_;
    int                                                     port_;
    std::shared_ptr<ServerHandler>                          handler_;
    std::shared_ptr<apache::thrift::ThriftServer>           server_;
    apache::thrift::util::ScopedServerThread                *serverThread_ {nullptr};
};

}  // namespace infra
