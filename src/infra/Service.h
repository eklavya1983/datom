#pragma once

#include <memory>
#include <infra/ModuleProvider.h>
#include <infra/ServiceServer.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen/gen-cpp2/ServiceApi.h>

namespace apache { namespace thrift { namespace server {
class TNonblockingServer;
}}}

namespace infra {

struct CoordinationClient;
struct ConnectionCache;

struct ServiceApiHandler : ServiceApiSvIf {
    using KVBMessageHandler = std::function<std::unique_ptr<KVBinaryData> (std::unique_ptr<KVBinaryData>)>;
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
    virtual void run();
    virtual void shutdown();

    const std::string& getServiceEntryKey() const;
    std::string getDatasphereId() const override;
    std::string getNodeId() const override;
    std::string getServiceId() const override;
    CoordinationClient* getCoordinationClient() const override;
    ConnectionCache*    getConnectionCache() const override;

    inline const std::string& getLogContext() const {
        return logContext_;
    }

    static Service* newDefaultService(const std::string &logContext,
                                      const ServiceInfo &info,
                                      const std::string &zkServers);

 protected:
    virtual void initCoordinationClient_();
    virtual void initServer_();
    virtual void ensureDatasphereMembership_();
    virtual void publishServiceInfomation_();

    std::string                                 logContext_;
    ServiceInfo                                 serviceInfo_;
    std::string                                 serviceEntryKey_;
    /* client for coordination services */
    std::shared_ptr<CoordinationClient>         coordinationClient_;
    /* Connection cache */
    std::shared_ptr<ConnectionCache>            connectionCache_;
    /* Server */
    std::shared_ptr<ServiceServer>              server_;

    friend struct ServiceApiHandler;
};

}
