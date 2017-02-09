#include <volumeserver/VolumeMetaDb.h>
#include <folly/futures/Future.h>
#include <util/Log.h>
#include <infra/Serializer.tcc>
#include <infra/StatusException.h>
#include <infra/gen/gen-cpp2/volumeapi_types.h>
#include <infra/gen/gen-cpp2/volumeapi_types.tcc>
#include <infra/gen/gen-cpp2/status_types.h>

namespace volume {
using namespace infra;

struct BlobMetaOffsetKey {
    BlobMetaOffsetKey() = default;
    BlobMetaOffsetKey(const datomdb::Slice &slice)
    {
        auto buf = reinterpret_cast<const int64_t*>(slice.data());
        data[0] = buf[0];
        data[1] = buf[1];
    }
    BlobMetaOffsetKey(int64_t blobId, int64_t offset)
    {
        data[0] = blobId;
        data[1] = offset;
    }
    void setBlobId(int64_t blobId) {
        data[0] = blobId;
    }
    int64_t getBlobId() const {
        return data[0];
    }
    void setOffset(int64_t offset) {
        data[1] = offset;
    }
    int64_t getOffset() const {
        return data[1];
    }
    datomdb::Slice toSlice() const {
        return datomdb::Slice(reinterpret_cast<const char*>(data), sizeof(data));
    }

    int64_t data[2];
};

VolumeMetaDb::VolumeMetaDb(const std::string &logCtx,
                           const std::string &dbPath)
    : logContext_(logCtx),
    dbPath_(dbPath)
{
}

VolumeMetaDb::~VolumeMetaDb()
{
    if (db_) delete db_;
}

void VolumeMetaDb::init()
{
    datomdb::Options options;
    options.create_if_missing = true;
    datomdb::Status status =
        datomdb::DB::Open(options, dbPath_, &db_);
    if (!status.ok()) {
        CLog(ERROR) << "Failed to load db at path:" << dbPath_;
        throw StatusException(Status::STATUS_RESOURCE_LOAD_FAILED);
    }
    CLog(INFO) << "Successfully initialized db at path:" << dbPath_;
}

folly::Future<std::unique_ptr<UpdateBlobMetaRespMsg>>
VolumeMetaDb::updateBlobMeta(const std::unique_ptr<UpdateBlobMetaMsg> &msg,
                             const std::unique_ptr<folly::IOBuf> &buffer) 
{
    /* TODO(Rao): blob existence checks */

    /* Create a batch */
#if 0
    // TODO(Rao): Batch size can be preallocated for efficiency
    // This needs to be taken into account for large blobs especially
    datomdb::WriteBatch batch(
        (sizeof(key) + commontypes_constants::CHUNKID_SIZE()) * msg->chunkList.size());
#endif
    CVLog(LIO) << "updateBlobMeta:" << toJsonString(*msg);

    datomdb::WriteBatch batch;

    BlobMetaOffsetKey key;
    key.setBlobId(msg->blobId);
    for (const auto &kv : msg->chunkList) {
        key.setOffset(kv.offset);
        datomdb::Slice sliceVal(kv.chunkId.data(), kv.chunkId.size());
        batch.Put(key.toSlice(), sliceVal);
    }

    /* Apply batch */
    auto status = db_->Write(datomdb::WriteOptions(), &batch);
    if (!status.ok()) {
        CLog(WARNING) << "Failed to updateBlobMeta" << toJsonString(*msg);
        return folly::makeFuture<std::unique_ptr<UpdateBlobMetaRespMsg>>(
            StatusException(Status::STATUS_DB_WRITE_FAILURE));
    }

    return folly::makeFuture(std::make_unique<UpdateBlobMetaRespMsg>());
}

folly::Future<std::unique_ptr<GetBlobMetaRespMsg>>
VolumeMetaDb::getBlobMeta(const std::unique_ptr<GetBlobMetaMsg> &msg)
{
    auto resp = std::make_unique<GetBlobMetaRespMsg>();
    resp->resourceId = msg->resourceId;
    resp->blobId = msg->blobId;
    std::unique_ptr<datomdb::Iterator> it(db_->NewIterator(datomdb::ReadOptions()));
    BlobMetaOffsetKey startKey(msg->blobId, msg->startOffset);
    for (it->Seek(startKey.toSlice()); it->Valid(); it->Next()) {
        BlobMetaOffsetKey offsetKey(it->key());
        if (offsetKey.getBlobId() != msg->blobId ||
            (msg->endOffset != -1 && offsetKey.getOffset() > msg->endOffset)) {
            break;
        }
        BlobKVPair pair;
        pair.offset = offsetKey.getOffset();
        pair.chunkId = std::string(it->value().data(), it->value().size());
        resp->chunkList.push_back(pair);
    }
    return folly::makeFuture(std::move(resp));
}

}  // namespace volume
