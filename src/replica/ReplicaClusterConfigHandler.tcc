#pragma once
#include <folly/io/async/EventBase.h>
#include <folly/futures/Future.h>
#include <infra/CoordinationClient.h>
#include <replica/ReplicaClusterConfigHandler.h>

namespace replica {

template <class ServiceT, class ResourceT>
ReplicaClusterConfigHandler<ServiceT, ResourceT>::
ReplicaClusterConfigHandler(folly::EventBase *eb,
                            CoordinationClient *coordinationClient,
                            const std::string &name)
{
    eb_ = eb;
    coordinationClient_ = coordinationClient;
    name_ = name;
}

template <class ServiceT, class ResourceT>
folly::Future<Status> ReplicaClusterConfigHandler<ServiceT, ResourceT>::
addService(const ServiceT &info)
{
    DCHECK(eb_->isInEventBaseThread());
    pendingServices_.push_back(info);

    /* Enough replicas to create a ring */
    if (pendingServices_.size() == replicationFactor_) {
        std::vector<std::string> serviceIds;
        for (const auto &info : pendingServices_) {
            serviceIds.push_back(info.id);
        }
        return addRing(serviceIds);
    } else {

    }
}

}  // namespace replica
