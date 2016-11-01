#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <infra/CoordinationClient.h>

extern "C" {
typedef struct _zhandle zhandle_t;
}

namespace infra {

struct KafkaClient;
enum class Status;

/**
 * @brief C++ zookeeper kafka client
 */
struct ZooKafkaClient : CoordinationClient {
    /* Maximum connection retries against zookeeper servers */
    const static int MAX_CONN_TRIES = 1;
    static void watcherFnGlobal(zhandle_t *zh,
                                int type,
                                int state,
                                const char *path,
                                void *watcherCtx);
    static void watcherFn(zhandle_t *zh,
                          int type,
                          int state,
                          const char *path,
                          void *watcherCtx);
    ZooKafkaClient(const std::string &logContext,
                   const std::string& servers,
                   const std::string& consumerGroupId="");
    ~ZooKafkaClient();
    void init() override;

    folly::Future<std::string> create(const std::string &key,
                                      const std::string &value) override;
    folly::Future<std::string> createIncludingAncestors(const std::string &key,
                                                        const std::string &value) override;
    folly::Future<std::string> createEphemeral(const std::string &key,
                                               const std::string &value,
                                               bool sequential = false) override;
    folly::Future<int64_t> set(const std::string &key,
                               const std::string &value,
                               const int &version) override;
    folly::Future<KVBinaryData> get(const std::string &key) override;
    folly::Future<std::vector<std::string>>
        getChildrenSimple(const std::string &key,
                          const WatchCb &watchCb=nullptr) override;
#if 0
    folly::Future<std::vector<KVBinaryData>> getChildren(const std::string &key) override;
#endif
    std::vector<KVBinaryData> getChildrenSync(const std::string &key) override;
#if 0
    folly::Future<std::string> put(const std::string &key,
                                   const std::string &value) override;
#endif

    Status publishMessage(const std::string &topic,
                       const std::string &message) override;
    Status subscribeToTopic(const std::string &topic, const MsgReceivedCb &cb) override;

    static std::string typeToStr(int type);
    static std::string stateToStr(int state);
    void watcher(int type, int state, const char *path);

    zhandle_t* getHandle() { return zh_; }

    inline std::string getLogContext() const { return logContext_; }

 protected:
    void blockUntilConnectedOrTimedOut_(int seconds);
    folly::Future<std::string> createCommon_(const std::string &key,
                                             const std::string &value,
                                             int flags);

    std::string                                     logContext_;
    /* List of zookeeper servers */
    std::string                                     servers_;
    /* Zookeeper handle */
    zhandle_t                                       *zh_ {nullptr};
    /* Kafka client handle */
    std::shared_ptr<KafkaClient>                    kafkaClient_;
};

}  // namespace infra
