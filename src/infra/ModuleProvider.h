#include <string>
#include <memory>

namespace folly {
class EventBase;
}

namespace infra {
struct CoordinationClient;
struct ConnectionCache;
struct NodeRoot;
struct ServiceApiHandler;

struct ModuleProvider {
    virtual std::string getDatasphereId() const { return ""; }
    virtual std::string getNodeId() const { return ""; }
    virtual std::string getServiceId() const { return ""; }
    virtual CoordinationClient* getCoordinationClient() const { return nullptr; }
    virtual ConnectionCache*    getConnectionCache() const { return nullptr; }
    virtual folly::EventBase*   getEventBaseFromPool() { return nullptr; }
    virtual NodeRoot*           getNodeRoot() {return nullptr; }
    virtual std::shared_ptr<ServiceApiHandler>  getServiceApiHandler() { return nullptr; }
};

}  // namespace infra
