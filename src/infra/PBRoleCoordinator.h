
namespace infra {

struct PBRoleCoordinatorListener {
};

struct PBRoleCoordinator {
    PBRoleCoordinator(PBRoleCoordinatorListener *listener,
                const std::string &leaderKey,
                const std::vector<std::string> &members,
                const std::string &me);
    void init();
    bool amILeader();
};

}  // namespace infra
