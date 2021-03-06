#include <util/Log.h>
#include <wangle/concurrent/IOThreadPoolExecutor.h>
#include <infra/ZooKafkaClient.h>
#include <infra/Service.h>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/ServiceServer.h>
#include <infra/ConnectionCache.h>
#include <infra/gen-ext/KVBuffer_ext.tcc>
#include <infra/StatusException.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <infra/gen/gen-cpp2/service_constants.h>
#include <infra/gen/gen-cpp2/commontypes_types.tcc>

namespace infra {

void ServiceApiHandler::getModuleState(std::string& _return,
                                       std::unique_ptr<std::map<std::string, std::string>> arguments)
{
    _return = "ok";
}

folly::Future<std::unique_ptr<KVBuffer>>
ServiceApiHandler::future_handleKVBMessage(std::unique_ptr<KVBuffer> message)
{
    auto type = getType(*message);
    auto itr = kvbMessageHandlers_.find(type);
    if (itr == kvbMessageHandlers_.end()) {
        // CLog(WARNING) << "Unknow type:" << type << " fro KVBMessage.  Ignoring message";
        throw apache::thrift::TApplicationException(
            folly::sformat("No handler registered for type:{}", type));
    }
    return itr->second(std::move(message));
}


void ServiceApiHandler::registerKVBMessageHandler(const std::string &type,
                                                  const KVBMessageHandler &handler)
{
    kvbMessageHandlers_[type] = handler;
}

NodeRoot::NodeRoot(const std::string &basePath)
    : basePath_(basePath)
{}

std::string NodeRoot::getVolumesPath()
{
    return folly::sformat(service_constants::FS_VOLUMES_PATH(), basePath_);
}

std::string NodeRoot::getDataPath()
{
    return folly::sformat(service_constants::FS_DATA_PATH(), basePath_);
}

void NodeRoot::makeNodeRootTree()
{
    std::system(folly::sformat("mkdir -p {}", basePath_).c_str());
    /* Create volumes */
    std::system(folly::sformat("mkdir -p {}", getVolumesPath()).c_str());
    /* Create data */
    std::system(folly::sformat("mkdir -p {}", getDataPath()).c_str());
}

void NodeRoot::cleanNodeRootTree()
{
    std::system(folly::sformat("rm -rf {}", basePath_).c_str());
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
    serviceInfo_(info),
    nodeRoot_(info.rootPath)
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
    shutdown();
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

void Service::run(bool async)
{
    CLog(INFO) << "Starting server mode:" << (async ? "own thread" : "main thread");
    if (async) {
        serverThread_.reset(new std::thread([this]() { server_->start(); }));
    } else {
        server_->start();
    }
}

void Service::shutdown()
{
    CLog(INFO) << "Shutting down service";
    /* We destruct iothreadpool inorder to circumvent the restiction that object
     * using event base be cleaned up on event base if even base is running.
     * NOTE all eventbase based object require this, some such as AsyncSocket
     * require it.
     */
    if (ioThreadpool_) {
        ioThreadpool_.reset();
    }

    if (server_) {
        server_->stop();
        if (serverThread_) {
            serverThread_->join();
        }
        server_.reset();
        serverThread_.reset();
    }
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

folly::EventBase* Service::getEventBaseFromPool()
{
    return ioThreadpool_->getEventBase();
}

std::shared_ptr<ServiceApiHandler> Service::getServiceApiHandler()
{
    return getHandler<ServiceApiHandler>();
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

void Service::initIOThreadpool_(int nIOThreads)
{
    ioThreadpool_ = std::make_shared<wangle::IOThreadPoolExecutor>(nIOThreads);
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
    if (f.hasException()) {
        CLog(ERROR) << " failed to publish service information at key:"
            << getServiceEntryKey();
        /* Rethrow the exception */
        f.value();
        return;
    }

    /* Publish the new service information to service topic */
    KVBuffer kvb;
    kvb.payload = folly::IOBuf::copyBuffer(payload.data(), payload.size());
    setVersion(kvb, f.value());
    payload = serializeToThriftJson<>(kvb, getLogContext());
    coordinationClient_->publishMessage(configtree_constants::TOPIC_SERVICES(),
                                        payload);

    CLog(INFO) << "Published service information to ConfigDb at key:"
        << getServiceEntryKey()
        << " topic:" << configtree_constants::TOPIC_SERVICES();
}

}  // namespace infra
