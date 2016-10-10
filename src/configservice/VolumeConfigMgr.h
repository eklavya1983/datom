#include <infra/gen/status_types.h>

namespace folly {
template <class T>
class Future;
struct Unit;
}

namespace infra {
struct VolumeInfo;
struct ServiceInfo;

using StatusFuture = folly::Future<Status>;
}

namespace config {

using namespace infra;

/**
 * @brief Responsible for configuration of volumes
 */
struct VolumeConfigMgr {
    virtual ~VolumeConfigMgr() = default;
    virtual StatusFuture addVolumeService(const ServiceInfo &info); 
    virtual StatusFuture addVolume(const VolumeInfo&) = 0;
};

}  // namespace config
