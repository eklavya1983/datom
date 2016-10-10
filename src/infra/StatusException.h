#pragma once
#include <exception>
#include <infra/gen/gen-cpp2/status_types.h>

namespace infra {

/**
 * @brief Exception that carries status codes from status.thrift
 */
struct StatusException : std::exception {
    StatusException()
        : StatusException(Status::STATUS_OK) {}
    explicit StatusException(Status status)
        : status_(status),
        originalStatus_(static_cast<int>(status))
    {
    }

    inline void setStatus(Status status)
    {
        status_ = status;
    }

    inline Status getStatus() const {
        return status_;
    }

    inline void setOriginalStatus(int status) {
        originalStatus_ = status;
    }

    inline int getOriginalStatus() const {
        return originalStatus_;
    }

    const char* what() const noexcept override
    {
        return _Status_VALUES_TO_NAMES.at(status_);
    }

 private:
    Status status_;
    int originalStatus_;
};

}  // namespace infra
