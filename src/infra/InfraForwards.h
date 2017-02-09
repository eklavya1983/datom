#pragma once

namespace folly {
template <class T>
class Future;
template <class T>
class Promise;
struct Unit;
class EventBase;
class IOBuf;
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
using VoidPromise = folly::Promise<folly::Unit>;
struct CoordinationClient;
struct ConnectionCache;
class ServiceInfo;
class KVBuffer;

namespace at = apache::thrift;
namespace ata = apache::thrift::async;
}
