#pragma once

#include <unordered_map>
#include <memory>
#include <infra/InfraForwards.h>
#include <infra/Service.h>
#include <folly/SharedMutex.h>

namespace config {

struct DatasphereConfigMgr;
using DatasphereConfigMgrSPtr = std::shared_ptr<DatasphereConfigMgr>;

using namespace infra;

struct ConfigService : Service {
    using DatasphereConfigTable = std::unordered_map<std::string, DatasphereConfigMgrSPtr>;

    ConfigService(const std::string &logContext,
                  const ServiceInfo &info,
                  const std::shared_ptr<CoordinationClient> &coordinationClient);

    virtual ~ConfigService();

    void init() override;

    /* NOTE: Below methods don't do checks for existence.  It is intentional as
     * this code is just testing.  It is up to the user to ensure this isn't
     * exploited
     */
    void createDatom();
    void addDataSphere(const DataSphereInfo &info);
    void addService(const ServiceInfo &info);
    void startVolumeCluster();
    void startDataCluster();

    /**
     * @brief Adds volume to configdb
     *
     * @param info
     *
     * @return Volume info with created volume id
     */
    VolumeInfo addVolume(const VolumeInfo &info);

    std::vector<infra::ServiceInfo> listServices(const std::string &datasphereId);
    std::vector<infra::VolumeInfo> listVolumes(const std::string &datasphereId);
    std::vector<infra::RingInfo> listVolumeRings(const std::string& datasphere);

 protected:
    DatasphereConfigTable::iterator getDatasphereOrThrow_(const std::string &id);
    void ensureDatasphereMembership_() override;
    void createPublishableTopics_();

    bool                                                        datomConfigured_ {false};
    folly::SharedMutex                                          datasphereMutex_;
    DatasphereConfigTable                                       datasphereTable_;
};

}  // namespace config
