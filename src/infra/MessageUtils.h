#pragma once

#include <folly/io/IOBuf.h>

namespace infra {

extern thread_local std::unique_ptr<folly::IOBuf>       threadLocalBuffer;

}  // namespace infra
