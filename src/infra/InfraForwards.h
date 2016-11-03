#pragma once

namespace folly {
template <class T>
class Future;
struct Unit;
class EventBase;
}

namespace wangle {
class IOThreadPoolExecutor;
}

namespace apache { namespace thrift {
class HeaderClientChannel;
namespace async {
class TAsyncSocket;
}
}}


namespace infra {
enum class Status;
using StatusFuture = folly::Future<Status>;
using VoidFuture = folly::Future<folly::Unit>;
struct CoordinationClient;
struct ConnectionCache;
class ServiceInfo;
class KVBinaryData;

namespace at = apache::thrift;
namespace ata = apache::thrift::async;
}
