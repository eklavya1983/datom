#pragma once
#include <infra/Db.h>
#include <infra/InfraForwards.h>

namespace infra {
enum class Status;
}

namespace volume {
class UpdateBlobMetaMsg;
class UpdateBlobMetaRespMsg;
class GetBlobMetaMsg;
class GetBlobMetaRespMsg;

struct VolumeMetaDb {
    VolumeMetaDb(const std::string &logCtx,
                 const std::string &dbPath);
    ~VolumeMetaDb();
    const std::string& getLogContext() const { return logContext_; }
    void init();

    infra::Status applyUpdate(const std::unique_ptr<UpdateBlobMetaMsg> &msg,
                              UpdateBlobMetaRespMsg *resp);
    folly::Future<std::unique_ptr<GetBlobMetaRespMsg>>
    getBlobMeta(const std::unique_ptr<GetBlobMetaMsg> &msg);

 protected:
    std::string             logContext_;
    std::string             dbPath_;
    datomdb::DB             *db_{nullptr};
};
}
