#pragma once

#include <string>
#include <vector>

namespace folly {
class EventBase;
}

namespace infra {

struct PBRoleCoordinatorListener {
};

struct PBRoleCoordinator {
    PBRoleCoordinator(folly::EventBase *eb,
                      PBRoleCoordinatorListener *listener,
                      const std::string &leaderKey,
                      const std::vector<std::string> &members,
                      const std::string &me);
    void init();
    bool amILeader();
};

}  // namespace infra
