#include "Thread.h"

void Thread::join(const Thread& thread) {
    const_cast<Thread&>(thread).join();
}

TID Thread::self() {
    return ::pthread_self();
}

string Thread::toString() const {
    StringStream message;
    message << "Thread[" << (ulong)(uintptr_t)getTID() << "]";
    return message.toString();
}
