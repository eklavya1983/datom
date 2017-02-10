#pragma once
#include <volumeserver/VolumeServer.h>

namespace volume {

template<class ReqT, class RespT>
folly::Future<std::unique_ptr<RespT>>
VolumeReplica::handleMetadataWrite(std::unique_ptr<ReqT> msg,
                                   std::unique_ptr<folly::IOBuf> buffer)
{
    DCHECK(eb_->isInEventBaseThread());
    Status s = journal_.addEntry(msg->commitId, typeStr<ReqT>(), *buffer);
    if (s != Status::STATUS_OK) {
        throw StatusException(s);
    }
    auto resp = std::make_unique<RespT>();
    s = db_.applyUpdate(msg, resp.get());
    if (s != Status::STATUS_OK) {
        journal_.removeLastEntry();
        throw StatusException(s);
    }
    return folly::makeFuture(std::move(resp));
}

}  // namespace volume
