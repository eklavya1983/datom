#pragma once

#include <cstdlib>
#include <exception>
#include <zookeeper/zookeeper.h>
#include <testlib/DatomBringupHelper.h>
#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <infra/ZooKafkaClient.h>
#include <infra/Service.h>
#include <infra/Serializer.tcc>
#include <infra/gen/gen-cpp2/configtree_constants.h>
#include <infra/gen-ext/commontypes_ext.h>
#include <infra/ConnectionCache.h>
#include <infra/StatusException.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <volumeserver/VolumeServer.h>

namespace testlib {
using namespace infra;
using namespace volume;

template <class ConfigServiceT>
DatomBringupHelper<ConfigServiceT>::DatomBringupHelper()
{
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::stopServices()
{
    if (configService_) {
        configService_.reset();
    }
    for (auto &kv : volumeServers_) {
        kv.second.reset();
    }

}
template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::runServices()
{
    for (auto &kv : volumeServers_) {
        kv.second->run(true);
    }
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::cleanStartDatom()
{
    std::system("rm -rf /simulation/*");

    /* Bringup zookeeper and kafka */
    KafkaRunner_.cleanstart();

    /* Bringup  config service */
    auto zkClient = std::make_shared<ZooKafkaClient>("ConfigService",
                                                     "localhost:2181/datom",
                                                     "ConfigService");
    configService_ = std::make_shared<ConfigServiceT>("ConfigService",
                                                      ServiceInfo(),
                                                      zkClient);
    configService_->init();

    /* Create datom namespace */
    configService_->createDatom();
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::cleanStopDatom()
{
    stopServices();
    KafkaRunner_.cleanstop();
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::shutdownDatom()
{
    stopServices();
    KafkaRunner_.stop();
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::addDataSphere(const std::string &dataSphereId)
{
    DataSphereInfo info;
    info.id = dataSphereId;
    configService_->addDataSphere(info);
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::addService(const std::string &dataSphereId,
                                    const std::string &nodeId,
                                    const std::string &serviceId,
                                    const std::string &ip,
                                    const int port)
{
    ServiceInfo info;
    info.id = serviceId;
    info.nodeId = nodeId;
    info.dataSphereId = dataSphereId;
    info.ip = ip;
    info.port = port;
    configService_->addService(info);
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::startService(const std::string &datasphereId,
                                                      const std::string &serviceId)
{
    auto info = configService_->getServiceInfo(datasphereId, serviceId);
    CLog(INFO) << "Starting service " << info;
    auto configClient = std::make_shared<ZooKafkaClient>(info.id,
                                                         "localhost:2181/datom",
                                                         info.id);

    auto service = std::make_shared<VolumeServer>(info.id,
                                                  info,
                                                  std::make_shared<ServiceApiHandler>(),
                                                  configClient);
    service->init();
    if (info.type == ServiceType::VOLUME_SERVER) {
        auto id = folly::sformat("{}:{}", datasphereId, info.id);
        volumeServers_[id] = service;
    }
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::stopService(const std::string &datasphereId,
                                                     const std::string &serviceId)
{
}

template <class ConfigServiceT>
ServiceInfo DatomBringupHelper<ConfigServiceT>::generateServiceInfo(
    const std::string &datasphereId,
    int nodeIdx,
    const ServiceType &type) 
{
    static int basePort = 2085;
    ServiceInfo serviceInfo;
    serviceInfo.dataSphereId = datasphereId;
    serviceInfo.nodeId = folly::sformat("node{}", nodeIdx);
    if (type == ServiceType::VOLUME_SERVER) {
        serviceInfo.id = folly::sformat("volumeserver{}", nodeIdx);
    } else {
        serviceInfo.id = folly::sformat("service{}", nodeIdx);
    }
    serviceInfo.type = type;
    serviceInfo.ip = "127.0.0.1";
    serviceInfo.port = basePort + nodeIdx*10;
    serviceInfo.rootPath = folly::sformat("/simulation/datom/{}", serviceInfo.nodeId);
    return serviceInfo;
}

template <class ConfigServiceT>
ServiceInfo DatomBringupHelper<ConfigServiceT>::generateVolumeServiceInfo(
    const std::string &datasphereId,
    int nodeIdx) 
{
    return generateServiceInfo(datasphereId, nodeIdx, ServiceType::VOLUME_SERVER);
}

template <class ConfigServiceT>
void DatomBringupHelper<ConfigServiceT>::
createPrimaryBackupDatasphere(const std::string &datasphereId,
                              int32_t numNodes)
{
    DataSphereInfo datasphere;
    datasphere.id = datasphereId;
    configService_->addDataSphere(datasphere);

    // TODO(Rao): Add nodes.  For now skipping adding nodes and directily adding
    // services

    std::vector<ServiceInfo> serviceInfos;
    for (int i = 0; i < numNodes; i++) {
        auto serviceInfo = generateVolumeServiceInfo(datasphereId, i);
        configService_->addService(serviceInfo);
        serviceInfos.push_back(serviceInfo);
        NodeRoot(serviceInfo.rootPath).makeNodeRootTree();
    }

    for (const auto &info : serviceInfos) {
        CLog(INFO) << "Starting service " << info;
        auto configClient = std::make_shared<ZooKafkaClient>(info.id,
                                                             "localhost:2181/datom",
                                                             info.id);
             
        auto service = std::make_shared<VolumeServer>(info.id,
                                                      info,
                                                      std::make_shared<ServiceApiHandler>(),
                                                      configClient);
        service->init();
        auto id = folly::sformat("{}:{}", datasphereId, info.id);
        volumeServers_[id] = service;
    }
}

template <class ConfigServiceT>
ScopedDatom<ConfigServiceT>::ScopedDatom(DatomBringupHelper<ConfigServiceT>& d)
    : datom_(d)
{
    datom_.cleanStartDatom();
}

template <class ConfigServiceT>
ScopedDatom<ConfigServiceT>::~ScopedDatom()
{
    datom_.cleanStopDatom();
}


}  // namespace testlib 
