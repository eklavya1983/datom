#include <configservice/RedundantVolumeConfigMgr.h>
#include <configservice/ConfigService.h>
#include <folly/futures/Future.h>
#include <folly/futures/Future-inl.h>
#include <folly/io/async/EventBase.h>

namespace config {

struct  VolumeRedundancyRing {
    std::vector<std::string> members;
};


RedundantVolumeConfigMgr::RedundantVolumeConfigMgr(const std::string &logContext,
                                                   ConfigService *configService)
    : logContext_(logContext),
    configService_(configService)
{
}

StatusFuture RedundantVolumeConfigMgr::addVolumeService(const ServiceInfo &info)
{
    return runInEventBase([this, info] {
    });
}

StatusFuture RedundantVolumeConfigMgr::addVolume(const VolumeInfo& info)
{
    return via(eb_).then([this, info]() {
    });
    /* Determine placement */
    /* Create the volume */
    /* Set volume id */
    /* Publish volume creation */
}

folly::Future<Status> RedundantVolumeConfigMgr::addVolume(const VolumeInfo&)
{
}

}  // namespace config
