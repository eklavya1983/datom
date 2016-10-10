#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <thread>
#include <vector>

namespace folly {
template <class T>
class Future;
struct Unit;
}

namespace RdKafka {
class Event;
class Producer;
class KafkaConsumer;
class Topic;
class Message;
class TopicPartition;
}

namespace infra {

struct KafkaEventCb;
struct KafkaRebalanceCb;
enum class Status;

/**
 * @brief Kafka client
 */
struct KafkaClient {
    using MsgReceivedCb = std::function<void (int64_t, const std::string &)>;

    KafkaClient(const std::string logContext,
                const std::string &brokers,
                const std::string &consumerGroupId);
    virtual ~KafkaClient();
    virtual void init();

    Status publishMessage(const std::string &topic,
                       const std::string &message);
    Status subscribeToTopic(const std::string &topic, const MsgReceivedCb &cb);

    void eventCallback(RdKafka::Event &event);
    void rebalanceCallback(RdKafka::KafkaConsumer *consumer,
                           int err,
                           std::vector<RdKafka::TopicPartition*> &partitions);

    inline const std::string& getLogContext() const { return logContext_; }

 protected:
    void consumeMessage_(RdKafka::Message* message, void* opaque);

    std::string                                         logContext_;
    std::string                                         brokers_;
    std::string                                         consumerGroupId_;
    RdKafka::Producer                                   *producer_ {nullptr};
    RdKafka::KafkaConsumer                              *consumer_ {nullptr};
    KafkaEventCb                                        *eventCb_ {nullptr}; 
    KafkaRebalanceCb                                    *rebalanceCb_ {nullptr};
    std::thread                                         *consumeThread_ {nullptr};

    std::mutex                                          subscriptionLock_;
    std::unordered_map<std::string, MsgReceivedCb>      subscriptionCbs_;

    std::mutex                                          publishTopicsLock_;
    std::unordered_map<std::string, std::shared_ptr<RdKafka::Topic>>    publishTopics_;
    bool                                                aborted_ {false};
};

}  // namespace infra
