#pragma once
#include <infra/Db.h>
#include <infra/InfraForwards.h>

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

    folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
    updateBlobMeta(const std::unique_ptr<UpdateBlobMetaMsg> &msg,
                   const std::unique_ptr<folly::IOBuf> &buffer);

    folly::Future<std::unique_ptr<GetBlobMetaRespMsg>>
    getBlobMeta(const std::unique_ptr<GetBlobMetaMsg> &msg);

 protected:
    std::string             logContext_;
    std::string             dbPath_;
    datomdb::DB             *db_{nullptr};
};
}
