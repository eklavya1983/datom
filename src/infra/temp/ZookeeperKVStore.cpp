#include <ZookeeperKVStore.h>
#include <zk/zkadapter.h>

namespace cluster {


ZookeeperKVStore::ZookeeperKVStore(const std::string &logContext,
                                   const std::string &zkServers)
    : logContext_(logContext),
    zkServers_(zkServers)
{
}

void ZookeeperKVStore::init()
{
    zkHandle_ = zookeeper_init(zkServers_.c_str(), 
}

void ZookeeperKVStore::refreshCache_()
{
}

std::string ZookeeperKVStore::getCachedValue(const std::string &key)
{
    return cache_.at(key);
}

folly::Future<std::string> ZookeeperKVStore::getCurrentValue(const std::string &key)
{

}

folly::Future<folly::Unit> ZookeeperKVStore::put(const std::string &key, const std::string &value)
{
}

void ZookeeperKVStore::watch(const std::string &key, const WatchCb &cb)
{
}

}  // namespace cluster
