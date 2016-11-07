#include <infra/KafkaClient.h>
#include <folly/futures/Future.h>
#include <librdkafka/rdkafkacpp.h>
#include <util/Log.h>
#include <infra/gen/gen-cpp2/status_types.h>

namespace infra {

struct KafkaEventCb : RdKafka::EventCb {
    KafkaEventCb(KafkaClient *parent)
        : parent_(parent)
    {
    }

    void event_cb (RdKafka::Event &event) 
    {
        parent_->eventCallback(event);
    }
    KafkaClient         *parent_ {nullptr};
};

struct KafkaRebalanceCb : public RdKafka::RebalanceCb {
  KafkaRebalanceCb(KafkaClient *parent)
      : parent_(parent)
  {
  }

  void rebalance_cb (RdKafka::KafkaConsumer *consumer,
                     RdKafka::ErrorCode err,
                     std::vector<RdKafka::TopicPartition*> &partitions) {
      parent_->rebalanceCallback(consumer, err, partitions);
  }

  KafkaClient         *parent_ {nullptr};
};


KafkaClient::KafkaClient(const std::string logContext,
                         const std::string &brokers,
                         const std::string &consumerGroupId)
{
    logContext_ = logContext;
    brokers_ = brokers;
    consumerGroupId_ = consumerGroupId;
}

KafkaClient::~KafkaClient()
{
    aborted_ = true;
    if (consumeThread_) {
        consumeThread_->join();
        delete consumeThread_;
    }

    if (producer_) {
        delete producer_;
    }

    if (consumer_) {
        consumer_->close();
        delete consumer_;
    }

    if (eventCb_) delete eventCb_;
    if (rebalanceCb_) delete rebalanceCb_;

    RdKafka::wait_destroyed(1000);

    CLog(INFO) << "Exiting KafkaClient";
}

void KafkaClient::init()
{
    /* Setup configuration */
    eventCb_ = new KafkaEventCb(this);
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    std::string errstr;
    conf->set("metadata.broker.list", brokers_, errstr);
    conf->set("event_cb", eventCb_, errstr);

    /* Initialize publisher */
    producer_ = RdKafka::Producer::create(conf.get(), errstr);
    if (!producer_) {
        CLog(ERROR) << "Unable to create producer";
        throw std::runtime_error("Unable to create producer");
    }
    CLog(INFO) << "Created producer against brokers:" << brokers_;

    /* Initialize subscriber */
    auto status = conf->set("group.id",  consumerGroupId_, errstr);
    CHECK(status == RdKafka::Conf::CONF_OK);
    rebalanceCb_ = new KafkaRebalanceCb(this);
    status = conf->set("rebalance_cb", rebalanceCb_, errstr);
    CHECK(status == RdKafka::Conf::CONF_OK);
    consumer_ = RdKafka::KafkaConsumer::create(conf.get(), errstr);
    if (!consumer_) {
        CLog(ERROR) << "Unable to create consumer";
        throw std::runtime_error("Unable to create consumer");
    }
    CLog(INFO) << "Created subscriber brokers:" << brokers_
        << " consumergroupid:" << consumerGroupId_;

    /* Start listening for subscribed messages */
    consumeThread_ = new std::thread([this]() {
        CLog(INFO) << "Subscribe loop started";
        while (!aborted_) {
            std::unique_ptr<RdKafka::Message> msg(consumer_->consume(1000));
            consumeMessage_(msg.get(), NULL);
        }
        CLog(INFO) << "Subscribe loop ended";
    });
}

Status KafkaClient::publishMessage(const std::string &topic,
                                const std::string &message)
{
    // TODO(Rao): It's possible to not use lock here by having KafkaClient run
    // on event base.  Consider it as an optimization
    std::shared_ptr<RdKafka::Topic> t;
    {
        std::lock_guard<std::mutex> lock(publishTopicsLock_);
        auto itr = publishTopics_.find(topic);
        if (itr == publishTopics_.end()) {
            std::string errstr;
            t.reset(RdKafka::Topic::create(producer_, topic,
                                           nullptr, errstr));
            if (!t) {
                CLog(WARNING) << "Failed to create topic:" << topic << " error:" << errstr;
                return Status::STATUS_PUBLISH_FAILED;
            }
            publishTopics_[topic] = t;
            CLog(INFO) << "Started publishing to new topic:" << topic;
        } else {
            t = itr->second;
        }
    }

    /* Empty message means just create the topic */
    if (message.empty()) {
        return Status::STATUS_OK;
    }
    
    RdKafka::ErrorCode resp =
        producer_->produce(t.get(), RdKafka::Topic::PARTITION_UA,
                           RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                           const_cast<char *>(message.c_str()), message.size(),
                           NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR) {
        CLog(WARNING) << "Failed to publish message to topic:" << topic
            << " error:" << RdKafka::err2str(resp);
        return Status::STATUS_PUBLISH_FAILED;
    }
    return Status::STATUS_OK;
}

Status KafkaClient::subscribeToTopic(const std::string &topic, const MsgReceivedCb &cb)
{
    std::lock_guard<std::mutex> lock(subscriptionLock_);
    auto itr = subscriptionCbs_.find(topic);
    if (itr != subscriptionCbs_.end()) {
        CLog(WARNING) << "Already subscribed to topic:" << topic << " ignoring request";
        return Status::STATUS_INVALID;
    }
    subscriptionCbs_[topic] = cb;
    // TODO(Rao): Figure out if there is a better way to subscribe to new topics
    // than like this where unsubscribe all and subscribe again
    std::vector<std::string> topics;
    for (const auto &kv : subscriptionCbs_) {
        topics.push_back(kv.first);
    }
    RdKafka::ErrorCode err = consumer_->subscribe(topics);
    if (err) {
        CLog(WARNING) << "Failed to subscribe to topic:" << topic
            << " error:" << RdKafka::err2str(err);
        return Status::STATUS_SUBSCRIBE_FAILED;
    }
    CLog(INFO) << "Subscribed to topic:" << topic;
    return Status::STATUS_OK;
}

void KafkaClient::consumeMessage_(RdKafka::Message* message, void* opaque)
{
    switch (message->err()) {
        case RdKafka::ERR__TIMED_OUT:
            break;

        case RdKafka::ERR_NO_ERROR:
        {
            auto topic = message->topic_name();
            auto itr = subscriptionCbs_.find(topic);
            if (itr == subscriptionCbs_.end()) {
                CLog(WARNING) << "Received message for topic:" << topic
                    << " that doesn't have registered callback.  Ignoring";
                break;
            }
            itr->second(message->offset(),
                        std::string(static_cast<const char *>(message->payload()),
                                    message->len()));
        }
#if 0
            /* Real message */
            msg_cnt++;
            msg_bytes += message->len();
            if (verbosity >= 3)
                std::cerr << "Read msg at offset " << message->offset() << std::endl;
            RdKafka::MessageTimestamp ts;
            ts = message->timestamp();
            if (verbosity >= 2 &&
                ts.type != RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
                std::string tsname = "?";
                if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
                    tsname = "create time";
                else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
                    tsname = "log append time";
                CLog(INFO) << "Timestamp: " << tsname << " " << ts.timestamp << std::endl;
            }
            if (verbosity >= 2 && message->key()) {
                CLog(INFO) << "Key: " << *message->key() << std::endl;
            }
            if (verbosity >= 1) {
                printf("%.*s\n",
                       static_cast<int>(message->len()),
                       static_cast<const char *>(message->payload()));
            }
#endif
            break;

        case RdKafka::ERR__PARTITION_EOF:
            /* Last message */
            break;

        case RdKafka::ERR__UNKNOWN_TOPIC:
        case RdKafka::ERR__UNKNOWN_PARTITION:
            CLog(WARNING) << "consume failed: " << message->errstr() << std::endl;
            break;

        default:
            /* Errors */
            CLog(WARNING) << "consume failed: " << message->errstr() << std::endl;
    }
}

void KafkaClient::eventCallback(RdKafka::Event &event)
{
    switch (event.type())
    {
        case RdKafka::Event::EVENT_ERROR:
            // NOTE: Connection error is reporte here as Broker transport
            // failure) TODO(Rao): Handle it
            CLog(WARNING) << "ERROR (" << RdKafka::err2str(event.err()) << "): " <<
                event.str() << std::endl;
            break;

        case RdKafka::Event::EVENT_STATS:
            CLog(INFO) << "\"STATS\": " << event.str() << std::endl;
            break;

        case RdKafka::Event::EVENT_LOG:
            CLog(INFO) << "EVENT_LOG:" << event.severity()
                << "-" << event.fac() << "-" << event.str();
            break;

        case RdKafka::Event::EVENT_THROTTLE:
            CLog(INFO) << "THROTTLED: " << event.throttle_time() << "ms by " <<
                event.broker_name() << " id " << (int)event.broker_id() << std::endl;
            break;

        default:
            CLog(INFO) << "EVENT " << event.type() <<
                " (" << RdKafka::err2str(event.err()) << "): " <<
                event.str() << std::endl;
            break;
    }
}

void KafkaClient::rebalanceCallback(RdKafka::KafkaConsumer *consumer,
                                    int err,
                                    std::vector<RdKafka::TopicPartition*> &partitions)
{

    CLog(INFO) << "RebalanceCb: " << RdKafka::err2str(static_cast<RdKafka::ErrorCode>(err))
        << ": ";

    for (unsigned int i = 0 ; i < partitions.size() ; i++) {
        CLog(INFO) << "RebalanceCb: " << partitions[i]->topic() <<
            "[" << partitions[i]->partition() << "], ";
    }

    if (err == RdKafka::ERR__ASSIGN_PARTITIONS) {
        consumer->assign(partitions);
    } else {
        consumer->unassign();
    }
}

}
