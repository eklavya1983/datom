#pragma once

namespace infra {
template <class MutexT>
struct SharedLock {
    explicit SharedLock(MutexT &m)
        : mutex_(m)
    {
        mutex_.lock_shared();
    }
    ~SharedLock()
    {
        mutex_.unlock_shared();
    }

 private:
    MutexT &mutex_;
};
}  // namespace infra
