#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <folly/futures/Future.h>
#include <volumeserver/VolumeMetaDb.h>
#include <infra/gen/gen-cpp2/VolumeApi.h>


using namespace infra;
using namespace volume;

/**
 * @brief Helper to setup and teardown db setup
 */
struct Helper {
    Helper() {
        setUp();
    }
    ~Helper() {
        tearDown();
    }
    void setUp() {
        std::system("mkdir dbs");
    }
    void tearDown() {
        std::system("rm -rf dbs");
    }
};

TEST(VolumeMetaDb, updateBlobMeta_basic)
{
    Helper h;
    VolumeMetaDb db("test", "dbs/db1");
    db.init();

    auto blobMeta = std::make_unique<UpdateBlobMetaMsg>();
    blobMeta->blobId = 0;
    blobMeta->resourceId = 0;
    for (uint32_t i = 0; i < 5; i++) {
        BlobKVPair pair;
        pair.offset = i;
        pair.chunkId = folly::sformat("{}",i);
        blobMeta->chunkList.push_back(pair);
    }
    /* Basic write */
    auto updResp = db.updateBlobMeta(blobMeta, nullptr).get();
    ASSERT_TRUE(updResp != nullptr);

    /* Basic read */
    auto getMsg = std::make_unique<GetBlobMetaMsg>();
    getMsg->blobId = 0;
    getMsg->resourceId = 0;
    getMsg->startOffset = 0;
    getMsg->endOffset = -1;
    auto getResp = db.getBlobMeta(getMsg).get();
    ASSERT_TRUE(getResp->chunkList.size() == 5);
    for (uint32_t i = 0; i < getResp->chunkList.size(); i++) {
        ASSERT_EQ(i, getResp->chunkList[i].offset);
        ASSERT_EQ(folly::sformat("{}", i), getResp->chunkList[i].chunkId);
    }

    /* Read a range */
    getMsg->blobId = 0;
    getMsg->resourceId = 0;
    getMsg->startOffset = 2;
    getMsg->endOffset = 3;
    getResp = db.getBlobMeta(getMsg).get();
    ASSERT_TRUE(getResp->chunkList.size() == 2);
    for (uint32_t i = 0; i < getResp->chunkList.size(); i++) {
        ASSERT_EQ(i+2, getResp->chunkList[i].offset);
        ASSERT_EQ(folly::sformat("{}", i+2), getResp->chunkList[i].chunkId);
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto ret = RUN_ALL_TESTS();
    return ret;
}
