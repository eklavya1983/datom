#include <zookeeper/zookeeper.h>
#include <infra/ZooKafkaClient.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>
#include <infra/gen-ext/KVBinaryData_ext.tcc>
#include <chrono>
#include <thread>
#include <util/Log.h>
#include <folly/futures/Future.h>
#include <folly/Format.h>
#include <infra/KafkaClient.h>
#include <infra/gen/gen-cpp2/status_types.h>
#include <infra/StatusException.h>

namespace infra {

/**
 * @brief Convert zkError to StatusException
 *
 * @param zkError
 *
 * @return 
 */
static StatusException toStatusException(int zkError)
{
    StatusException e;
    e.setOriginalStatus(zkError);
    // TODO(Rao): Enhance by adding other mappings
    switch (zkError) {
        case ZNONODE:
            e.setStatus(Status::STATUS_INVALID_KEY);
            break;
        case ZNODEEXISTS:
            e.setStatus(Status::STATUS_DUPLICATE_KEY);
            break;
        default:
            e.setStatus(Status::STATUS_INVALID);
    }
    return e;
}

struct CreateCtx {
    folly::Promise<std::string> promise;
    std::string key;
};

struct GetChildrenSimpleCtx {
    folly::Promise<std::vector<std::string>> promise;
    std::string key;
};

struct GetCtx {
    folly::Promise<KVBinaryData> promise;
    std::string key;
};

struct SetCtx {
    folly::Promise<int64_t> promise;
    std::string key;
};

struct DeleteCtx {
    folly::Promise<folly::Unit> promise;
    std::string key;
};

struct WatchCtx {
    CoordinationClient::WatchCb cb;
};

static void stringVectorCompletionCb(int rc,
                                     const String_vector *strings,
                                     const void *data)
{
    GetChildrenSimpleCtx *ctx= const_cast<GetChildrenSimpleCtx*>(reinterpret_cast<const GetChildrenSimpleCtx*>(data));

    if (rc != ZOK) {
        LOG(WARNING) << "stringVectorCompletionCb key:" << ctx->key
            << " error:" << zerror(rc);
        ctx->promise.setException(infra::toStatusException(rc));
    } else {
        std::vector<std::string> resp;
        for (int i = 0; strings && i < strings->count; i++) {
            resp.push_back(std::string(strings->data[i]));
        }
        ctx->promise.setValue(std::move(resp));
    }
    delete ctx;
}

static void stringCompletionCb(int rc,
                               const char *value,
                               const void *data)
{
    CreateCtx *ctx = const_cast<CreateCtx*>(reinterpret_cast<const CreateCtx*>(data));

    if (rc != ZOK) {
        LOG(WARNING) << "stringCompletionCb key:" << ctx->key << " error:" << zerror(rc);
        ctx->promise.setException(infra::toStatusException(rc));
    } else {
        ctx->promise.setValue(std::string(value));
    }
    delete ctx;
}

static void dataCompletionCb(int rc, const char *value, int value_len,
                             const Stat *stat, const void *data)
{
    GetCtx *ctx = const_cast<GetCtx*>(reinterpret_cast<const GetCtx*>(data));
    if (rc != ZOK) {
        LOG(WARNING) << "dataCompletionCb key:" << ctx->key << " error:" << zerror(rc);
        ctx->promise.setException(infra::toStatusException(rc));
    } else {
        KVBinaryData kvb;
        setVersion(kvb, stat->version);
        kvb.data = std::move(std::string(value, value_len));
        ctx->promise.setValue(kvb);
    }
    delete ctx;
}

static void statCompletionCb(int rc, const struct Stat *stat, const void *data)
{
    SetCtx *ctx = const_cast<SetCtx*>(reinterpret_cast<const SetCtx*>(data));

    if (rc != ZOK) {
        LOG(WARNING) << "statCompletionCb key:" << ctx->key << " error:" << zerror(rc);
        ctx->promise.setException(infra::toStatusException(rc));
    } else {
        ctx->promise.setValue(stat->version);
    }
    delete ctx;
}

void voidCompletionCb(int rc, const void *data)
{
    DeleteCtx *ctx = const_cast<DeleteCtx*>(reinterpret_cast<const DeleteCtx*>(data));

    if (rc != ZOK) {
        LOG(WARNING) << "voidCompletionCb key:" << ctx->key << " error:" << zerror(rc);
        ctx->promise.setException(infra::toStatusException(rc));
    } else {
        ctx->promise.setValue();
    }
    delete ctx;
}

void ZooKafkaClient::watcherFnGlobal(zhandle_t *zh,
                                int type,
                                int state,
                                const char *path,
                                void *watcherCtx)
{
    static_cast<ZooKafkaClient*>(watcherCtx)->watcher(type, state, path);
}

void ZooKafkaClient::watcherFn(zhandle_t *zh,
                                int type,
                                int state,
                                const char *path,
                                void *watcherCtx)
{
    auto *ctx = static_cast<WatchCtx*>(watcherCtx);
    ctx->cb(path);
    delete ctx;
}



ZooKafkaClient::ZooKafkaClient(const std::string &logContext,
                               const std::string& servers,
                               const std::string &consumerGroupId)
    : logContext_(logContext),
    servers_(servers)
{
    if (!consumerGroupId.empty()) {
        // TODO(Rao): Don't hardcode to localhost
        kafkaClient_ = std::make_shared<KafkaClient>(logContext,
                                                     "localhost",
                                                     consumerGroupId);
    }
}

ZooKafkaClient::~ZooKafkaClient()
{
    if (zh_) {
        int rc = zookeeper_close(zh_);
        if (rc != ZOK) {
            CLog(WARNING) << "Failed to close zookeeper client.  error: " << zerror(rc);
        }
    }
    CLog(INFO) << "Exiting ZooKafkaClient";
}

void ZooKafkaClient::init()
{
    while (true) {
        zh_ = zookeeper_init(servers_.c_str(), &ZooKafkaClient::watcherFnGlobal, 30000, 0, this, 0);
        if (zh_ == nullptr) {
            CLog(FATAL) << "Failed to initialize zookeeper instance";
            throw toStatusException(ZINVALIDSTATE );
        }
        try {
            // TODO(Rao): We don't need to do this.  Connection can take place in the
            // backgroud
            blockUntilConnectedOrTimedOut_(5);
            break;
        } catch (std::exception &e) {
            CLog(INFO) << "Establishing connection failed.  Trying again";
            zookeeper_close(zh_);
        }
    }

    if (kafkaClient_) {
        kafkaClient_->init();
        CLog(INFO) << "Initialized kafka";
    } else {
        CLog(INFO) << "kafka client is not enabled";
    }
}

void ZooKafkaClient::blockUntilConnectedOrTimedOut_(int seconds)
{
    int secondsElapsed = 0;
    int state = 0;
    while (true) {
        state = zoo_state(zh_);
        if (state == ZOO_CONNECTED_STATE ||
            state == 0 ||  // NOT_CONNTECTED state
            secondsElapsed > seconds) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        secondsElapsed += 2;
        CLog(INFO) << "Zookeeper waiting to connect.  Current state:" << stateToStr(state);
    }

    if (zoo_state(zh_) != ZOO_CONNECTED_STATE) {
        CLog(ERROR) << "Failed to connect to zookeeper servers";
        throw toStatusException(ZINVALIDSTATE );
    }
}

void ZooKafkaClient::watcher(int type, int state, const char *path)
{
    CLog(INFO) << "Zookeeper watcher cb type:" << typeToStr(type)
        << " state:" << stateToStr(state)
        << " path:" << path;
}

std::string ZooKafkaClient::typeToStr(int type)
{
    if (type == ZOO_CREATED_EVENT)
        return "CREATED_EVENT";
    if (type == ZOO_DELETED_EVENT)
        return "DELETED_EVENT";
    if (type == ZOO_CHANGED_EVENT)
        return "CHANGED_EVENT";
    if (type == ZOO_CHILD_EVENT)
        return "CHILD_EVENT";
    if (type == ZOO_SESSION_EVENT)
        return "SESSION_EVENT";
    if (type == ZOO_NOTWATCHING_EVENT)
        return "NOTWATCHING_EVENT";

    return "UNKNOWN_EVENT_TYPE";
}

std::string ZooKafkaClient::stateToStr(int state)
{
    if (state == 0)
        return "CLOSED_STATE";
    if (state == ZOO_CONNECTING_STATE)
        return "CONNECTING_STATE";
    if (state == ZOO_ASSOCIATING_STATE)
        return "ASSOCIATING_STATE";
    if (state == ZOO_CONNECTED_STATE)
        return "CONNECTED_STATE";
    if (state == ZOO_EXPIRED_SESSION_STATE)
        return "EXPIRED_SESSION_STATE";
    if (state == ZOO_AUTH_FAILED_STATE)
        return "AUTH_FAILED_STATE";

    return "INVALID_STATE";
}

folly::Future<std::string> ZooKafkaClient::createCommon_(const std::string &key,
                                                         const std::string &value,
                                                         int flags)
{
    CreateCtx *ctx = new CreateCtx();
    ctx->key = key;
    auto future = ctx->promise.getFuture();
    auto rc = zoo_acreate(zh_, key.c_str(), value.c_str(), value.length(),
                              &ZOO_OPEN_ACL_UNSAFE, flags, &stringCompletionCb, ctx);
    if (rc != ZOK) {
        CLog(WARNING) << "failed to put key: " << key << " error: " << zerror(rc);
        delete ctx;
        return folly::makeFuture<std::string>(toStatusException(rc));
    }
    return future;
}

folly::Future<std::string> ZooKafkaClient::create(const std::string &key,
                                                  const std::string &value)
{
    CVLog(2) << "key:" << key << " value:" << value;

    return createCommon_(key, value, 0);
}

folly::Future<std::string> ZooKafkaClient::createIncludingAncestors(const std::string &key,
                                                                    const std::string &value)
{
    CVLog(2) << "key:" << key << " value:" << value;

    auto f = folly::makeFuture(std::string());
    for (std::string::size_type pos = 1; pos != std::string::npos; ) {
        pos = key.find( "/", pos );
        std::string path;
        if (pos != std::string::npos) {
            path = key.substr(0, pos);
            pos++;
        } else {
            path = key;
        }
        f = f.then([this, path, value](folly::Try<std::string> t) {
               try {
                    // if previous path creation failed below statement will
                    // throw.  We ignore Status::STATUS_DUPLICATE_KEY
                    (void) t.value();
               } catch (const StatusException &e) {
                    if (e.getStatus() != Status::STATUS_DUPLICATE_KEY) {
                        CLog(INFO) << path << " already exists.  Ignoring create";
                        throw e;
                    }
               }
               return create(path, value);
        });
    }
    return f;
}

folly::Future<std::string> ZooKafkaClient::createEphemeral(const std::string &key,
                                                           const std::string &value,
                                                           bool sequential)
{
    CVLog(2) << "key:" << key << " value:" << value << " sequential:" << sequential;

    if (sequential) {
        return createCommon_(key, value, ZOO_EPHEMERAL|ZOO_SEQUENCE);
    } else {
        return createCommon_(key, value, ZOO_EPHEMERAL);
    }
}

folly::Future<int64_t> ZooKafkaClient::set(const std::string &key,
                                           const std::string &value,
                                           const int &version)
{
    CVLog(2) << "key:" << key << " value:" << value << " version:" << version;

    SetCtx *ctx = new SetCtx();
    ctx->key = key;
    auto future = ctx->promise.getFuture();
    auto rc = zoo_aset(zh_, key.c_str(), value.c_str(), value.length(), version,
                       &statCompletionCb, ctx);
    if (rc != ZOK) {
        CLog(WARNING) << "failed to set key: " << key << " error: " << zerror(rc);
        delete ctx;
        return folly::makeFuture<int64_t>(toStatusException(rc));
    }
    return future;
}

folly::Future<folly::Unit> ZooKafkaClient::del(const std::string &key,
                                               const int &version)
{
    CVLog(2) << "key:" << key << " version:" << version;

    DeleteCtx *ctx = new DeleteCtx();
    ctx->key = key;
    auto future = ctx->promise.getFuture();
    auto rc = zoo_adelete(zh_, key.c_str(), version, &voidCompletionCb, ctx);
    if (rc != ZOK) {
        CLog(WARNING) << "failed to delete key: " << key << " error: " << zerror(rc);
        delete ctx;
        return folly::makeFuture<folly::Unit>(toStatusException(rc));
    }
    return future;
}

folly::Future<KVBinaryData> ZooKafkaClient::get(const std::string &key)
{
    CVLog(2) << "key:" << key;

    GetCtx *ctx = new GetCtx();
    ctx->key = key;
    auto future = ctx->promise.getFuture();
    auto rc = zoo_awget(zh_, key.c_str(), nullptr, nullptr,
                        &dataCompletionCb, ctx);
    if (rc != ZOK) {
        CLog(WARNING) << "failed to get key: " << key << " error: " << zerror(rc);
        delete ctx;
        return folly::makeFuture<KVBinaryData>(toStatusException(rc));
    }
    return future;
}

folly::Future<std::vector<std::string>>
ZooKafkaClient::getChildrenSimple(const std::string &key,
                                  const WatchCb &watchCb)
{
    int rc = ZOK;
    GetChildrenSimpleCtx *ctx = new GetChildrenSimpleCtx();
    ctx->key = key;
    auto future = ctx->promise.getFuture();


    if (watchCb) {
        CVLog(2) << "key:" << key << " watch set";
        auto watchCtx = new WatchCtx;
        watchCtx->cb = watchCb;
        rc = zoo_awget_children(zh_, key.c_str(), &ZooKafkaClient::watcherFn, watchCtx,
                                &stringVectorCompletionCb, ctx);
    } else {
        CVLog(2) << "key:" << key;
        rc = zoo_awget_children(zh_, key.c_str(), nullptr, nullptr,
                                &stringVectorCompletionCb, ctx);
    }
    if (rc != ZOK) {
        CLog(WARNING) << "failed to get children key: " << key << " error: " << zerror(rc);
        delete ctx;
        return folly::makeFuture<std::vector<std::string>>(toStatusException(rc));
    }
    return future;
}

#if 0
// TODO(Rao): Implement this.  Need latest folly to get this working
folly::Future<std::vector<KVBinaryData>>
ZooKafkaClient::getChildren(const std::string &key)
{
    auto f = getChildrenSimple(key);
    f.then([this, key](const std::vector<std::string>& children) {
           std::vector<folly::Future<KVBinaryData>>fs;
           for (const auto &c : children) {
              fs.push_back(this->get(folly::sformat("{}/{}", key, c)));
           }
           return folly::collect(fs);
           // TODO: Map
    })
}
#endif

std::vector<KVBinaryData> ZooKafkaClient::getChildrenSync(const std::string &key)
{
    CVLog(2) << "key:" << key;

    std::vector<KVBinaryData> ret;
    auto children  = getChildrenSimple(key).get();
    for (const auto &c : children) {
        auto data = this->get(folly::sformat("{}/{}", key, c)).get();
        setId(data, c);
        ret.push_back(data);
    }
    return ret;
}

#if 0
folly::Future<std::string> ZooKafkaClient::put(const std::string &key,
                                               const std::string &value)
{
    throw std::runtime_error("not implemented");

    CreateCtx *promise = new CreateCtx();
    auto future = promise->getFuture();
    auto rc = zoo_acreate(zh_, key.c_str(), value.c_str(), value.length(),
                              &ZOO_OPEN_ACL_UNSAFE, 0, &stringCompletionCb, promise);
    if (rc != ZOK) {
        CLog(WARNING) << "failed to put key: " << key << " error: " << zerror(rc);
        delete promise;
        return folly::makeFuture<std::string>(toStatusException(rc));
    }
    return future;
}
#endif

Status ZooKafkaClient::publishMessage(const std::string &topic,
                                   const std::string &message)
{
    return kafkaClient_->publishMessage(topic, message);
}

Status ZooKafkaClient::subscribeToTopic(const std::string &topic, const MsgReceivedCb &cb)
{
    return kafkaClient_->subscribeToTopic(topic, cb);
}


}  // namespace infra
