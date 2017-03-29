#include <infra/Journal.h>
#include <infra/gen/gen-cpp2/status_types.h>

namespace infra {

Status Journal::addEntry(int64_t id, const MsgId &type, const folly::IOBuf &buffer)
{
    if (entries.size() > 0 && entries.back().id+1 != id) {
        return Status::STATUS_INVALID_COMMIT;
    }
    entries.push_back(Record{id, type, buffer.cloneAsValue()});
    return Status::STATUS_OK;
}

void Journal::removeLastEntry()
{
    entries.pop_back();
}

Status Journal::retrieveEntries(int64_t fromId,
                                int64_t toId,
                                int64_t maxBytesInResp,
                                std::vector<JournalEntry> &ret)
{
    for (auto itr = entries.rbegin(); itr != entries.rend(); itr++) {
        if (itr->id >= fromId && itr->id <= toId) {
            JournalEntry entry;
            entry.entryId = itr->id;
            entry.msgId = itr->type;
            entry.buffer = itr->buf.clone();
            ret.emplace_back(std::move(entry));
        }
    }
    return Status::STATUS_OK;
}

}  // namespace infra
