#pragma once
#include <infra/InfraForwards.h>
#include <memory>

namespace volume {

class UpdateBlobMsg;
class UpdateBlobRespMsg;

class UpdateBlobMetaMsg;
class UpdateBlobMetaRespMsg;

/**
 * @brief VolumeHandle interface
 */
struct VolumeHandleIf {
    virtual folly::Future<std::unique_ptr<UpdateBlobRespMsg>> updateBlob(std::unique_ptr<UpdateBlobMsg> msg) = 0;
    virtual folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>> updateBlobMeta(std::unique_ptr<UpdateBlobMetaMsg> msg) = 0;
};

}  // namespace volumeserver
