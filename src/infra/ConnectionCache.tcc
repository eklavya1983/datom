#pragma once
#include <infra/ConnectionCache.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>

namespace infra {

template <class ClientT>
folly::Future<std::shared_ptr<ClientT>>
ConnectionCache::getAsyncClient(const std::string &serviceId)
{
    return getHeaderClientChannel(serviceId)
        .then([](const std::shared_ptr<at::HeaderClientChannel>& channel) {
              return std::make_shared<ClientT>(channel);
        });
}

}
