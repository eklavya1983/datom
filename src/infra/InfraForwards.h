#pragma once

namespace folly {
template <class T>
class Future;
struct Unit;
struct EventBase;
}

namespace wangle {
struct IOThreadPoolExecutor;
}

namespace infra {
enum class Status;
using StatusFuture = folly::Future<Status>;
using VoidFuture = folly::Future<folly::Unit>;
struct CoordinationClient;
struct ServiceInfo;
}
