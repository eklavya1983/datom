#pragma once
#include <string>
#include <list>
#include <folly/io/IOBuf.h>

namespace infra {

enum class Status;

struct Journal
{
    using MsgType = std::string;
    struct Record {
        int64_t id;
        MsgType type;
        folly::IOBuf buf;
    };
    Status addEntry(int64_t id, const MsgType &type, const folly::IOBuf &buffer);
    void removeLastEntry();

 protected:
    std::list<Record> entries;
};

}  // namespace infra

