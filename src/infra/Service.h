#pragma once

#include <memory>
#include <infra/InfraForwards.h>
#include <infra/ModuleProvider.h>
#include <infra/ServiceServer.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen/gen-cpp2/ServiceApi.h>

namespace apache { namespace thrift { namespace server {
class TNonblockingServer;
}}}

namespace infra {

struct ServiceApiHandler : ServiceApiSvIf {
    using KVBMessageHandler = std::function<folly::Future<std::unique_ptr<KVBinaryData>> (std::unique_ptr<KVBinaryData>)>;
    void getModuleState(std::string& _return,
                        std::unique_ptr<std::map<std::string, std::string>> arguments) override;
    folly::Future<std::unique_ptr<KVBinaryData>> future_handleKVBMessage(std::unique_ptr<KVBinaryData> message) override;

    void registerKVBMessageHandler(const std::string &type, const KVBMessageHandler &handler);

 protected:
    std::unordered_map<std::string, KVBMessageHandler> kvbMessageHandlers_;
};

/**
 * @brief Base Service class
 */
struct Service : ModuleProvider {
    Service(const std::string &logContext,
            const ServiceInfo &info,
            const std::shared_ptr<ServerHandler> &handler,
            const std::shared_ptr<CoordinationClient> &coordinationClient);
    virtual ~Service();
    Service() = delete;
    Service(const Service&) = delete;
    void operator=(Service const &) = delete;

    virtual void init();
    virtual void run(bool async=false);
    virtual void shutdown();

    const std::string& getServiceEntryKey() const;
    std::string getDatasphereId() const override;
    std::string getNodeId() const override;
    std::string getServiceId() const override;
    CoordinationClient* getCoordinationClient() const override;
    ConnectionCache* getConnectionCache() const override;
    folly::EventBase* getEventBaseFromPool() override;

    template <class T>
    std::shared_ptr<T> getHandler() const {
        return std::dynamic_pointer_cast<T>(server_->getHandler());
    }

    inline const std::string& getLogContext() const {
        return logContext_;
    }

    static Service* newDefaultService(const std::string &logContext,
                                      const ServiceInfo &info,
                                      const std::string &zkServers);

 protected:
    virtual void initIOThreadpool_(int nIOThreads);
    virtual void initCoordinationClient_();
    virtual void initServer_();
    virtual void ensureDatasphereMembership_();
    virtual void publishServiceInfomation_();

    std::string                                     logContext_;
    ServiceInfo                                     serviceInfo_;
    std::string                                     serviceEntryKey_;
    /* client for coordination services */
    std::shared_ptr<CoordinationClient>             coordinationClient_;
    /* Connection cache */
    std::shared_ptr<ConnectionCache>                connectionCache_;
    /* Thread on which server will listen.  Only valid if async is set in run()*/
    std::unique_ptr<std::thread>                    serverThread_;
    /* Server */
    std::shared_ptr<ServiceServer>                  server_;
    /* IO threadpool */
    std::shared_ptr<wangle::IOThreadPoolExecutor>   ioThreadpool_;

    friend struct ServiceApiHandler;
};

}
