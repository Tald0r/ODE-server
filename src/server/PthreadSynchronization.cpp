#include <cerrno>
#include <cstring>

#include "pthreadAPI.h"

namespace {
void checkResult(int result) {
    if (result != 0)
        throw UnknownError(std::strerror(result), result);
}
} // namespace

// Pthread functions return an error number directly. They do not report
// failure through a negative result or the calling thread's errno.
void pthreadAPI::pthread_mutex_init_ex(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    checkResult(::pthread_mutex_init(mutex, attr));
}

void pthreadAPI::pthread_mutex_destroy_ex(pthread_mutex_t* mutex) {
    const int result = ::pthread_mutex_destroy(mutex);
    if (result == EBUSY)
        throw MutexException("The mutex is already locked.");
    checkResult(result);
}

void pthreadAPI::pthread_mutex_lock_ex(pthread_mutex_t* mutex) {
    const int result = ::pthread_mutex_lock(mutex);
    if (result == EINVAL)
        throw Error("The mutex is not properly initialized.");
    if (result == EDEADLK)
        throw MutexException("DEADLOCK - the mutex is already locked by the current thread.");
    checkResult(result);
}

void pthreadAPI::pthread_mutex_unlock_ex(pthread_mutex_t* mutex) {
    const int result = ::pthread_mutex_unlock(mutex);
    if (result == EINVAL)
        throw Error("The mutex is not properly initialized.");
    if (result == EPERM)
        throw MutexException("The current thread does not hold the mutex.");
    checkResult(result);
}

void pthreadAPI::pthread_mutex_trylock_ex(pthread_mutex_t* mutex) {
    const int result = ::pthread_mutex_trylock(mutex);
    if (result == EINVAL)
        throw Error("The mutex is not properly initialized.");
    if (result == EBUSY)
        throw MutexException("mutex already locked...");
    checkResult(result);
}

void pthreadAPI::pthread_mutexattr_init_ex(pthread_mutexattr_t* attr) {
    checkResult(::pthread_mutexattr_init(attr));
}

void pthreadAPI::pthread_mutexattr_destroy_ex(pthread_mutexattr_t* attr) {
    checkResult(::pthread_mutexattr_destroy(attr));
}

void pthreadAPI::pthread_cond_init_ex(pthread_cond_t* condition, pthread_condattr_t* attr) {
    checkResult(::pthread_cond_init(condition, attr));
}

void pthreadAPI::pthread_cond_destroy_ex(pthread_cond_t* condition) {
    const int result = ::pthread_cond_destroy(condition);
    if (result == EBUSY)
        throw CondVarException("conditional variable is busy now.");
    checkResult(result);
}

void pthreadAPI::pthread_cond_signal_ex(pthread_cond_t* condition) {
    checkResult(::pthread_cond_signal(condition));
}

void pthreadAPI::pthread_cond_wait_ex(pthread_cond_t* condition, pthread_mutex_t* mutex) {
    checkResult(::pthread_cond_wait(condition, mutex));
}

void pthreadAPI::pthread_cond_timedwait_ex(pthread_cond_t* condition, pthread_mutex_t* mutex,
                                           const struct timespec* deadline) {
    const int result = ::pthread_cond_timedwait(condition, mutex, deadline);
    if (result == ETIMEDOUT)
        throw CondVarException("timeout");
    if (result == EINTR)
        throw InterruptedException();
    checkResult(result);
}

void pthreadAPI::pthread_cond_broadcast_ex(pthread_cond_t* condition) {
    checkResult(::pthread_cond_broadcast(condition));
}

void pthreadAPI::pthread_condattr_init_ex(pthread_condattr_t* attr) {
    checkResult(::pthread_condattr_init(attr));
}

void pthreadAPI::pthread_condattr_destroy_ex(pthread_condattr_t* attr) {
    checkResult(::pthread_condattr_destroy(attr));
}
