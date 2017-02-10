#include <infra/Journal.h>
#include <infra/gen/gen-cpp2/status_types.h>

namespace infra {

Status Journal::addEntry(int64_t id, const MsgType &type, const folly::IOBuf &buffer)
{
    entries.push_back(Record{id, type, buffer.cloneAsValue()});
    return Status::STATUS_OK;
}

void Journal::removeLastEntry()
{
    entries.pop_back();
}

}  // namespace infra
