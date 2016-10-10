#pragma once
#include <configservice/VolumeConfigMgr.h>

namespace folly {
class EventBase;
}

namespace config {
struct ConfigService;

struct RedundantVolumeConfigMgr : VolumeConfigMgr {
    explicit RedundantVolumeConfigMgr(const std::string &logContext,
                                      ConfigService *configService,
                                      folly::EventBase *eb);
    void init();

    StatusFuture addVolumeService(const ServiceInfo &info) override; 
    StatusFuture addVolume(const VolumeInfo& info) override;

    inline const std::string& getLogContext() const {
        return logContext_;
    }

 protected:
    std::string                         logContext_;
    ConfigService                       *configService_;
    folly::EventBase                    *eb_;
};

}  // namespace config
