#pragma once
#include <string>
#include <memory>
#include <testlib/KafkaRunner.h>
#include <unordered_map>


namespace infra {
struct ServiceInfo;
struct DataSphereInfo;
struct CoordinationClient;
struct Service;
enum class ServiceType;
}


namespace testlib {
template <class ConfigServiceT>
struct DatomBringupHelper {
    DatomBringupHelper();
    DatomBringupHelper(const DatomBringupHelper &) = delete;
    void operator=(DatomBringupHelper const &) = delete;
    void stopServices();
    void cleanStartDatom();
    void cleanStopDatom();
    void shutdownDatom();
    void addDataSphere(const std::string &dataSphereId);
    void addService(const std::string &dataSphereId,
                    const std::string &nodeId,
                    const std::string &serviceId,
                    const std::string &ip,
                    const int port);
    inline ConfigServiceT* getConfigService() { return configService_.get(); }

    void createPrimaryBackupDatasphere(const std::string &datasphereId,
                                       int32_t numNodes);
    void startPrimaryBackupDatasphere(const std::string &datasphereId);

    std::string getLogContext() const { return "DatomBringupHelper"; }

    static infra::ServiceInfo generateServiceInfo(const std::string &datasphereId,
                                                  int nodeIdx,
                                                  const infra::ServiceType &type);
    static infra::ServiceInfo generateVolumeServiceInfo(const std::string &datasphereId,
                                                        int nodeIdx);
 protected:
    KafkaRunner                                     KafkaRunner_;
    std::shared_ptr<ConfigServiceT>                 configService_;
    std::unordered_map<std::string, std::shared_ptr<infra::Service>> services_;
};

/**
 * @brief RAII helper to clean start and clean stop datom
 */
template <class ConfigServiceT>
struct ScopedDatom {
    ScopedDatom(DatomBringupHelper<ConfigServiceT>& d);
    ~ScopedDatom();
    ScopedDatom(const ScopedDatom&) = delete;
    void operator=(ScopedDatom const &) = delete;

 private:
    DatomBringupHelper<ConfigServiceT> &datom_;
};

}  // namespace testlib
