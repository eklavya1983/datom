#pragma once
#include <infra/ConnectionCache.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>

namespace infra {

/* This class ensure HeaderClientChannel create in updateConnection_
 * is destroyed on eventbase.  This assumes that eventbase is running
 * when HeaderClientChannel is being destroyed.  In typical use case
 * that assumption is true.
 */
/* NOTE: This approach didn't work. I got a segfault I couldn't explain.  I
 * didn't have the time to pursue more
 */
template <class T>
struct EbDestroyer {
    void operator()(T *client) const {
        auto channel = client->getChannel();
        if (!channel) {
            delete client;
            return;
        }
        auto eb = channel->getEventBase();
        eb->runInEventBaseThread([client]() { delete client; });
    }
};


template <class ClientT>
folly::Future<std::shared_ptr<ClientT>>
ConnectionCache::getAsyncClient(const std::string &serviceId)
{
    return getHeaderClientChannel(serviceId)
        .then([](const std::shared_ptr<at::HeaderClientChannel>& channel) {
              // return std::make_shared<ClientT>(channel);
              return std::shared_ptr<ClientT>(new ClientT(channel), EbDestroyer<ClientT>());
        });
}

}
