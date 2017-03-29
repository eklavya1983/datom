#pragma once
#include <string>
#include <list>
#include <folly/io/IOBuf.h>
#include <infra/gen/gen-cpp2/commontypes_types.h>

namespace infra {

enum class Status;

struct Journal
{
    struct Record {
        int64_t id;
        MsgId type;
        folly::IOBuf buf;
    };
    Status addEntry(int64_t id, const MsgId &type, const folly::IOBuf &buffer);
    void removeLastEntry();
    Status retrieveEntries(int64_t fromId,
                           int64_t toId,
                           int64_t maxBytesInResp,
                           std::vector<JournalEntry> &journalEntries);

 protected:
    std::list<Record> entries;
};

}  // namespace infra

