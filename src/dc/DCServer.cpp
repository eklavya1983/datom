#pragma once

#include <volumeserver/VolumeHandleIf.h>


namespace volume {

struct DataStoreIf {
    folly::Future writeDataChunks() = 0;
};

struct DataShardHandle : DataStoreIf {
};
