#include <string>

namespace folly {
class EventBase;
}

namespace infra {
struct CoordinationClient;
struct ConnectionCache;
struct NodeRoot;

struct ModuleProvider {
    virtual std::string getDatasphereId() const { return ""; }
    virtual std::string getNodeId() const { return ""; }
    virtual std::string getServiceId() const { return ""; }
    virtual CoordinationClient* getCoordinationClient() const { return nullptr; }
    virtual ConnectionCache*    getConnectionCache() const { return nullptr; }
    virtual folly::EventBase*   getEventBaseFromPool() { return nullptr; }
    virtual NodeRoot*           getNodeRoot() {return nullptr; }
};

}  // namespace infra
