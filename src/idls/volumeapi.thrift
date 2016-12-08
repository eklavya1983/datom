include "commontypes.thrift"

namespace cpp volume

struct UpdateBlobMsg {
}

struct UpdateBlobRespMsg {
}

struct BlobKVPair {
    1: i64			offset;
    2: commontypes.ChunkId 	chunkId;

}

struct UpdateBlobMetaMsg {
    1: i64		resourceId;
    2: i64		blobId;
    3: i64 		opId;
    4: i64		commitId;
    5: list<BlobKVPair> chunkList;
}

struct UpdateBlobMetaRespMsg  {
}

struct GetBlobMetaMsg {
    1: i64		resourceId;
    2: i64		blobId;
    3: i64 		startOffset;
    4: i64 		endOffset;
}

struct GetBlobMetaRespMsg  {
    1: i64		resourceId;
    2: i64		blobId;
    3: list<BlobKVPair> chunkList;
}

/* Db keys */
struct BlobOffsetDbKey {
    1: i64		blobId;
    2: i64		offset;
}

service VolumeApi {
    UpdateBlobRespMsg updateBlob(1: UpdateBlobMsg msg)
    UpdateBlobMetaRespMsg  updateBlobMeta(1: UpdateBlobMetaMsg msg)
}
