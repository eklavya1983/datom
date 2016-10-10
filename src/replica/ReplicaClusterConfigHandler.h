#include <list>
#include <infra/gen/status_types.h>

namespace folly {
template <class T>
class Future;
struct Unit;
class EventBase;
}

namespace infra {
struct CoordinationClient;
}

namespace replica {
using namespace infra;

template <class ServiceT, class ResourceT>
struct ReplicaClusterConfigHandler {
    ReplicaClusterConfigHandler(folly::EventBase *eb,
                                CoordinationClient *coordinationClient,
                                const std::string &name);
    virtual void init(const int32_t &replicationFactor);
    virtual void load();
    virtual folly::Future<Status> addService(const ServiceT &info);
    virtual folly::Future<Status> addResource(const ResourceT &resource);

    virtual folly::Future<Status> addRing(const std::vector<std::string> &serviceIds);

 protected:
    folly::EventBase                    *eb_;
    CoordinationClient                  *coordinationClient_;
    std::string                         name_;
    int32_t                             replicationFactor_;
    std::list<ServiceT>                 pendingServices_;
};

}  // namespace replica
