include "commontypes.thrift"

namespace cpp volume

struct UpdateBlobMsg {
}

struct UpdateBlobRespMsg {
}

struct UpdateBlobMetaMsg {
    1: i64		resourceId;
    2: i64		blobId;
    3: i64		commitId;
}

struct UpdateBlobMetaRespMsg  {
}

service VolumeApi {
    UpdateBlobRespMsg updateBlob(1: UpdateBlobMsg msg)
    UpdateBlobMetaRespMsg  updateBlobMeta(1: UpdateBlobMetaMsg msg)
}
