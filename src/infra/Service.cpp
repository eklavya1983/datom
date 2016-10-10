#include <util/Log.h>
#include <infra/ZooKafkaClient.h>
#include <infra/Service.h>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/ServiceServer.h>
#include <infra/ConnectionCache.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <infra/StatusException.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>

namespace infra {

void ServiceApiHandler::getModuleState(std::string& _return,
                                       std::unique_ptr<std::map<std::string, std::string>> arguments)
{
    _return = "ok";
    LOG(INFO) << "returning hello";
}

Service* Service::newDefaultService(const std::string &logContext,
                                    const ServiceInfo &info,
                                    const std::string &zkServers)
{
    auto zkClient = std::make_shared<ZooKafkaClient>(logContext, zkServers, info.id);
    auto service = new Service(logContext, info, nullptr, zkClient);
    return service;
}

Service::Service(const std::string &logContext,
                 const ServiceInfo &info,
                 const std::shared_ptr<ServerHandler> &handler,
                 const std::shared_ptr<CoordinationClient> &coordinationClient)
    : logContext_(logContext),
    serviceInfo_(info)
{
    coordinationClient_ = coordinationClient;
    connectionCache_ = std::make_shared<ConnectionCache>(logContext_, this);
    if (handler) {
        server_ = std::make_shared<ServiceServer>(logContext,
                                                  serviceInfo_.ip,
                                                  serviceInfo_.port,
                                                  handler);
    }
}


Service::~Service()
{
    CLog(INFO) << "Exiting Service";
}


void Service::init()
{
    serviceEntryKey_ = folly::sformat(
        configtree_constants::SERVICE_ROOT_PATH_FORMAT(),
        getDatasphereId(), getServiceId()); 

    if (coordinationClient_) {
        initCoordinationClient_();

        /* ensure service is part of the data sphere, otherwise will throw */
        ensureDatasphereMembership_();
        /* fetch properties */
        /* init connection cache */
        connectionCache_->init();
    } else {
        CLog(INFO) << "Coordination client is disabled."
            << "  Not initializing CoordinationClient, ConnectionCache";
    }


    if (server_) {
        initServer_();
    } else {
        CLog(INFO) << "Server is disabled.  Not initializing server";
    }

    /* publish this service is up */
    if (coordinationClient_) {
        publishServiceInfomation_();
    }
}

void Service::run()
{
    server_->start();
}

void Service::shutdown()
{
    server_->stop();
}

const std::string& Service::getServiceEntryKey() const
{
    return serviceEntryKey_;
}

std::string Service::getDatasphereId() const
{
    return serviceInfo_.dataSphereId;
}

std::string Service::getNodeId() const
{
    return serviceInfo_.nodeId;
}

std::string Service::getServiceId() const
{
    return serviceInfo_.id;
}

CoordinationClient* Service::getCoordinationClient() const
{
    return coordinationClient_.get();
}

ConnectionCache* Service::getConnectionCache() const
{
    return connectionCache_.get();
}

void Service::initCoordinationClient_()
{
    /* Connect with zookeeper */
    coordinationClient_->init();
    CLog(INFO) << "Initialized CoordinationClient";
}

void Service::initServer_()
{
    // TODO(Rao): Any sort of intiting
}

void Service::ensureDatasphereMembership_()
{
    try {
        coordinationClient_->get(getServiceEntryKey()).get();
    } catch (const StatusException &e) {
        // TODO(Rao): We are leaking zookeeper specific error codes.  We
        // shouldn't
        if (e.getStatus() == Status::STATUS_INVALID_KEY) {
            CLog(ERROR) << "Service entry doesn't exist in ConfigDb";
        }
        throw;
    }
}

void Service::publishServiceInfomation_()
{
    /* Update config db with service information */
    std::string payload = serializeToThriftJson<>(serviceInfo_, getLogContext());
    auto f = coordinationClient_->set(getServiceEntryKey(), payload, -1);
    f.wait();

    /* Publish the new service information to service topic */
    KVBinaryData kvb;
    kvb.data = std::move(payload);
    setVersion(kvb, f.value());
    payload = serializeToThriftJson<>(kvb, getLogContext());
    coordinationClient_->publishMessage(configtree_constants::TOPIC_SERVICES(),
                                        payload);

    CLog(INFO) << "Published service information to ConfigDb";
}

}  // namespace infra
