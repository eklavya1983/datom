#pragma once
#include <infra/ConnectionCache.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>

namespace infra {

template <class ClientT>
folly::Future<std::shared_ptr<ClientT>>
ConnectionCache::getAsyncClient(const std::string &serviceId)
{
    return getAsyncSocket(serviceId)
        .then([](const std::shared_ptr<ata::TAsyncSocket>& socket) {
              auto channel = new at::HeaderClientChannel(socket);
              // channel->setTimeout(5000);
              return std::make_shared<ClientT>(std::unique_ptr<at::HeaderClientChannel,
                             folly::DelayedDestruction::Destructor>(channel));
        });
}

}
