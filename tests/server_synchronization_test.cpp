#include <unistd.h>

#include <barrier>
#include <cerrno>
#include <cstdlib>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>
#include <type_traits>

#include "CondVar.h"
#include "Mutex.h"
#include "MutexAttr.h"
#include "pthreadAPI.h"

namespace {
class NativeMutex {
public:
    NativeMutex() {
        pthread_mutexattr_t attr;
        if (::pthread_mutexattr_init(&attr) != 0 || ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0 ||
            ::pthread_mutex_init(&value, &attr) != 0 || ::pthread_mutexattr_destroy(&attr) != 0)
            std::_Exit(90);
    }
    ~NativeMutex() {
        ::pthread_mutex_destroy(&value);
    }
    pthread_mutex_t value;
};

TEST(PthreadMutex, BusyTryLockReportsTheReturnCodeWithoutUsingErrno) {
    ASSERT_EXIT(
        {
            NativeMutex mutex;
            if (::pthread_mutex_lock(&mutex.value) != 0)
                std::_Exit(91);
            bool refused = false;
            std::jthread contender([&] {
                errno = ENOENT;
                try {
                    pthreadAPI::pthread_mutex_trylock_ex(&mutex.value);
                } catch (const MutexException&) {
                    refused = true;
                }
            });
            contender.join();
            if (::pthread_mutex_unlock(&mutex.value) != 0)
                std::_Exit(92);
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PthreadMutex, RecursiveNativeLockReportsDeadlockWithoutUsingErrno) {
    ASSERT_EXIT(
        {
            NativeMutex mutex;
            if (::pthread_mutex_lock(&mutex.value) != 0)
                std::_Exit(91);
            errno = ENOENT;
            bool refused = false;
            try {
                pthreadAPI::pthread_mutex_lock_ex(&mutex.value);
            } catch (const MutexException&) {
                refused = true;
            }
            if (::pthread_mutex_unlock(&mutex.value) != 0)
                std::_Exit(92);
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PthreadMutex, NonOwnerUnlockReportsRefusalAndLeavesTheOwnerIntact) {
    ASSERT_EXIT(
        {
            NativeMutex mutex;
            if (::pthread_mutex_lock(&mutex.value) != 0)
                std::_Exit(91);
            bool refused = false;
            std::jthread other([&] {
                errno = ENOENT;
                try {
                    pthreadAPI::pthread_mutex_unlock_ex(&mutex.value);
                } catch (const MutexException&) {
                    refused = true;
                }
            });
            other.join();
            if (::pthread_mutex_unlock(&mutex.value) != 0)
                std::_Exit(92);
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerMutex, ContendedTryLockRefusesWithoutClaimingOwnership) {
    ASSERT_EXIT(
        {
            Mutex mutex;
            mutex.lock();
            bool refused = false;
            std::jthread contender([&] {
                try {
                    mutex.trylock();
                } catch (const Error&) {
                    refused = true;
                }
            });
            contender.join();
            mutex.unlock();
            std::jthread nextOwner([&] {
                mutex.trylock();
                mutex.unlock();
            });
            nextOwner.join();
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerMutex, ConditionWaitRestoresOwnershipBeforeRecursiveLockRefusal) {
    ASSERT_EXIT(
        {
            // A broken recursive refusal must fail this child, not hang ctest.
            ::alarm(3);
            Mutex mutex;
            CondVar condition;
            bool ready = false;
            mutex.lock();
            std::jthread notifier([&] {
                std::lock_guard held(mutex);
                ready = true;
                condition.signal();
            });
            while (!ready)
                condition.wait(mutex);
            bool refused = false;
            try {
                mutex.lock();
            } catch (const Error&) {
                refused = true;
            }
            mutex.unlock();
            notifier.join();
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PthreadCondition, SuccessfulDestructionDoesNotReportStaleErrnoAsFailure) {
    ASSERT_EXIT(
        {
            pthread_cond_t condition;
            if (::pthread_cond_init(&condition, nullptr) != 0)
                std::_Exit(93);
            errno = ENOENT;
            try {
                pthreadAPI::pthread_cond_destroy_ex(&condition);
            } catch (const Throwable&) {
                std::_Exit(1);
            }
            std::_Exit(0);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PthreadCondition, InvalidDeadlineReportsItsReturnCodeAndRetainsOwnership) {
    ASSERT_EXIT(
        {
            NativeMutex mutex;
            pthread_cond_t condition;
            if (::pthread_cond_init(&condition, nullptr) != 0 || ::pthread_mutex_lock(&mutex.value) != 0)
                std::_Exit(94);
            timespec invalid{};
            invalid.tv_nsec = 1000000000;
            errno = ENOENT;
            bool refused = false;
            try {
                pthreadAPI::pthread_cond_timedwait_ex(&condition, &mutex.value, &invalid);
            } catch (const UnknownError& error) {
                refused = error.getErrorCode() == EINVAL;
            }
            if (::pthread_mutex_unlock(&mutex.value) != 0 || ::pthread_cond_destroy(&condition) != 0)
                std::_Exit(95);
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(ServerMutex, NativeMutexAndAttributesCannotBeCopiedOrMoved) {
    EXPECT_FALSE(std::is_copy_constructible_v<Mutex>);
    EXPECT_FALSE(std::is_copy_assignable_v<Mutex>);
    EXPECT_FALSE(std::is_move_constructible_v<Mutex>);
    EXPECT_FALSE(std::is_move_assignable_v<Mutex>);
    EXPECT_FALSE(std::is_copy_constructible_v<MutexAttr>);
    EXPECT_FALSE(std::is_copy_assignable_v<MutexAttr>);
    EXPECT_FALSE(std::is_move_constructible_v<MutexAttr>);
    EXPECT_FALSE(std::is_move_assignable_v<MutexAttr>);
}

TEST(ServerMutex, RefusalDiagnosticsRetainTheConfiguredMutexName) {
    ASSERT_EXIT(
        {
            Mutex mutex;
            mutex.setName("named synchronization fixture");
            mutex.lock();
            bool refused = false;
            try {
                mutex.trylock();
            } catch (const Error& error) {
                refused = error.getMessage().find("named synchronization fixture") != std::string::npos;
            }
            mutex.unlock();
            std::_Exit(refused ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "named synchronization fixture");
}

TEST(ServerMutex, DefaultsRefuseRecursiveLockingAndNonOwnerUnlockWithoutLosingTheOwner) {
    Mutex mutex;
    mutex.lock();
    EXPECT_THROW(mutex.lock(), Error);
    EXPECT_THROW(mutex.trylock(), Error);
    bool refused = false;
    std::jthread other([&] {
        try {
            mutex.unlock();
        } catch (const Error&) {
            refused = true;
        }
    });
    other.join();
    EXPECT_TRUE(refused);
    EXPECT_NO_THROW(mutex.unlock());
    EXPECT_THROW(mutex.unlock(), Error);
    EXPECT_NO_THROW(mutex.trylock());
    mutex.unlock();
}

TEST(ServerMutex, SuppliedAttributesAreReusableAndExplicitRecursiveTypeIsHonored) {
    MutexAttr attributes;
    int type = -1;
    ASSERT_EQ(::pthread_mutexattr_gettype(attributes.getAttr(), &type), 0);
    EXPECT_EQ(type, PTHREAD_MUTEX_ERRORCHECK);
    {
        Mutex first(&attributes);
        Mutex second(&attributes);
        std::lock_guard firstLock(first);
        std::lock_guard secondLock(second);
        EXPECT_THROW(first.trylock(), Error);
        EXPECT_THROW(second.trylock(), Error);
    }
    ASSERT_EQ(::pthread_mutexattr_settype(attributes.getAttr(), PTHREAD_MUTEX_RECURSIVE), 0);
    Mutex recursive(&attributes);
    recursive.lock();
    EXPECT_NO_THROW(recursive.lock());
    EXPECT_NO_THROW(recursive.trylock());
    recursive.unlock();
    recursive.unlock();
    bool refused = false;
    std::jthread other([&] {
        try {
            recursive.trylock();
        } catch (const Error&) {
            refused = true;
        }
    });
    other.join();
    EXPECT_TRUE(refused);
    recursive.unlock();
    EXPECT_NO_THROW(recursive.trylock());
    recursive.unlock();
}

TEST(ServerMutex, CriticalSectionReleasesOwnershipAcrossStandardExceptionsAndEarlyReturns) {
    Mutex mutex;
    const auto guarded = [&](bool fail) {
        __ENTER_CRITICAL_SECTION(mutex)
        if (fail)
            throw std::runtime_error("work failed");
        return 42;
        __LEAVE_CRITICAL_SECTION(mutex)
    };
    EXPECT_THROW(guarded(true), std::runtime_error);
    EXPECT_EQ(guarded(false), 42);
    EXPECT_NO_THROW(mutex.trylock());
    mutex.unlock();
}

TEST(ServerMutex, ContendedHandoffsProtectEveryUpdate) {
    Mutex mutex;
    std::barrier ready(5);
    unsigned updates = 0;
    const auto update = [&] {
        ready.arrive_and_wait();
        for (unsigned index = 0; index < 1000; ++index) {
            std::lock_guard held(mutex);
            ++updates;
            if (index % 16 == 0)
                std::this_thread::yield();
        }
    };
    std::jthread workers[] = {std::jthread(update), std::jthread(update), std::jthread(update), std::jthread(update)};
    ready.arrive_and_wait();
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(updates, 4000u);
}

TEST(ServerCondition, TimeoutRetainsOwnershipAndAllowsSubsequentUse) {
    Mutex mutex;
    CondVar condition;
    std::unique_lock held(mutex);
    timespec deadline{};
    ASSERT_EQ(::clock_gettime(CLOCK_REALTIME, &deadline), 0);
    // An already expired deadline avoids a timing-dependent sleep.
    --deadline.tv_sec;
    EXPECT_THROW(condition.timedwait(mutex, &deadline), CondVarException);
    EXPECT_THROW(mutex.trylock(), Error);
    held.unlock();
    EXPECT_NO_THROW(mutex.trylock());
    mutex.unlock();
}

TEST(ServerCondition, BroadcastWakesAllWaitersUnderTheReacquiredMutex) {
    Mutex mutex;
    CondVarAttr attributes;
    CondVar condition(&attributes);
    std::promise<void> entered[2];
    auto firstEntered = entered[0].get_future();
    auto secondEntered = entered[1].get_future();
    bool released = false;
    unsigned woke = 0;
    const auto wait = [&](unsigned index) {
        std::lock_guard held(mutex);
        entered[index].set_value();
        while (!released)
            condition.wait(mutex);
        ++woke;
    };
    std::jthread first(wait, 0);
    std::jthread second(wait, 1);
    firstEntered.wait();
    secondEntered.wait();
    {
        std::lock_guard held(mutex);
        released = true;
        condition.broadcast();
    }
    first.join();
    second.join();
    EXPECT_EQ(woke, 2u);
}
} // namespace
