#pragma once
#include <infra/PBMember.h>
#include <folly/ExceptionWrapper.h>
#include <folly/futures/Future.h>
#include <folly/io/async/EventBase.h>
#include <infra/StatusException.h>

namespace infra {
template<class ReqT, class RespT, typename F>
folly::Future<std::unique_ptr<RespT>> PBMember::groupWriteInEb_(F &&localWriteFunc,
                                                                std::unique_ptr<ReqT> msg)
{
    DCHECK(eb_->isInEventBaseThread());
    if (!isLeaderState()) {
        CLog(WARNING) << "Attempt to write when not a leader.  Current state:"
            << _PBMemberState_VALUES_TO_NAMES.at(state_);
        throw StatusException(Status::STATUS_INVALID_STATE);
    }
    msg->opId = ++(leaderCtx_->opId);
    msg->commitId = ++(leaderCtx_->commitId);
    auto payload = serializeToBinary(*msg);
    /* First apply the write to primary.  If it succeeds apply to backups */ 
    auto f = localWriteFunc(std::move(msg), std::move(payload->clone()));
    return f
        .then([this, payload=std::move(payload)](std::unique_ptr<RespT> resp) mutable {
            /* We expect this then to execute immediately */
            DCHECK(eb_->isInEventBaseThread());
            return 
                writeToPeers(typeStr<ReqT>(), std::move(payload))
                .then([resp=std::move(resp)]() mutable {
                      return std::move(resp);
                });
        })
        .onError([this](const StatusException &e) {
            CLog(WARNING) << "Group write failed exception:" << e.what();
            return folly::makeFuture<std::unique_ptr<RespT>>(e);
        })
        .onError([this](folly::exception_wrapper ew) {
            CLog(WARNING) << "Group write failed exception:" << ew.what().toStdString();
            return folly::makeFuture<std::unique_ptr<RespT>>(ew);
        });
}
}
