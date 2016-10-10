#pragma once
#include <infra/StatusException.h>
namespace infra {
struct MessageException : StatusException {
    MessageException(Status status, const std::string &message)
        : StatusException(status),
        message_(message)
    {
    }
    void setMessage(const std::string &message)
    {
        message_ = message;
    }
    const std::string& getMessage() const
    {
        return message_;
    }
    const char* what() const noexcept override
    {
        return message_.c_str();
    }
 protected:
    std::string message_;
};
}
