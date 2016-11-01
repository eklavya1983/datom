#pragma once

#include <string>
#include <vector>
#include <functional>

namespace folly {
template <class T>
class Future;
struct Unit;
}

namespace infra {

struct KVBinaryData;
enum class Status;


struct CoordinationClient {
    using MsgReceivedCb = std::function<void (int64_t, const std::string &)>;
    using WatchCb = std::function<void(const std::string&)>;

    virtual void init() = 0;
    virtual folly::Future<KVBinaryData> get(const std::string &key) = 0;
    virtual folly::Future<std::vector<std::string>> getChildrenSimple(const std::string &key,
                                                                      const WatchCb &watchCb=nullptr) = 0;
    virtual std::vector<KVBinaryData> getChildrenSync(const std::string &key) = 0;
    virtual folly::Future<std::string> create(const std::string &key, const std::string &value) = 0;
    virtual folly::Future<std::string> createIncludingAncestors(const std::string &key,
                                                                const std::string &value) = 0;
    virtual folly::Future<std::string> createEphemeral(const std::string &key,
                                                       const std::string &value,
                                                       bool sequential = false) = 0;
    virtual folly::Future<int64_t> set(const std::string &key,
                                       const std::string &value,
                                       const int &version) = 0;
    
    virtual Status publishMessage(const std::string &topic,
                                  const std::string &message) = 0;
    virtual Status subscribeToTopic(const std::string &topic, const MsgReceivedCb &cb) = 0;
#if 0
    virtual folly::Future<std::string> put(const std::string &key,
                                           const std::string &value) = 0;
#endif
};

}  // namespace infra
