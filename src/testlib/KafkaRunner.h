#pragma once
#include <gflags/gflags.h>

DECLARE_string(toolsdir);

namespace testlib {

struct KafkaRunner {
    KafkaRunner();
    ~KafkaRunner()=default;
    void cleanstart();
    void cleanstop();
    void stop();

 protected:
    std::string             taskScript_;
};

struct ScopedKafkaRunner {
    ScopedKafkaRunner();
    ~ScopedKafkaRunner();
 protected:
    KafkaRunner runner_;
};

} // namespace testlib
