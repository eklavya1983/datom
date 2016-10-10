#include <memory>

namespace cluster {

struct KVStore;
struct PubSubMgr;
using KVStoreSPtr = std::shared_ptr<KVStore>;
using PubSubMgrSPtr = std::shared_ptr<PubSubMgr>;

/**
 * @brief Interface class for providing modules
 */
struct ModuleProvider {
    virtual KVStoreSPtr getKVStore() = 0;
    virtual PubSubMgrSPtr getPubSubMgr() = 0;
};

}  // namespace cluster
