#include <infra/MessageUtils.h>

namespace infra {
thread_local std::unique_ptr<folly::IOBuf>       threadLocalBuffer;
}  // namespace infra
