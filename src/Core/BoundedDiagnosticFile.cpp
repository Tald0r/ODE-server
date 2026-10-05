#include "BoundedDiagnosticFile.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#include <sys/file.h>
#include <sys/stat.h>

namespace de {
namespace {
class Descriptor {
public:
    explicit Descriptor(int value) : value(value) {}
    ~Descriptor() {
        if (value >= 0)
            ::close(value);
    }
    int value;
};

int openFile(const char* path, int flags) noexcept {
    int result;
    do {
        result = ::open(path, flags | O_CLOEXEC, 0640);
    } while (result < 0 && errno == EINTR);
    return result;
}

bool writeAll(int descriptor, std::string_view text) noexcept {
    while (!text.empty()) {
        const auto written = ::write(descriptor, text.data(), text.size());
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            if (written == 0)
                errno = EIO;
            return false;
        }
        text.remove_prefix(static_cast<std::size_t>(written));
    }
    return true;
}

bool fail(const char* operation, int error) noexcept {
    reportDiagnosticFileFailure(operation, error);
    return false;
}

bool renameExisting(const std::string& source, const std::string& destination) noexcept {
    int result;
    do {
        result = ::rename(source.c_str(), destination.c_str());
    } while (result < 0 && errno == EINTR);
    return result == 0 || errno == ENOENT;
}
} // namespace

void detail::writeDiagnosticStderr(std::string_view record) noexcept {
    sigset_t blocked, previous, pending;
    ::sigemptyset(&blocked);
    ::sigaddset(&blocked, SIGPIPE);
    if (::pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0)
        return;
    if (::sigpending(&pending) != 0) {
        ::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        return;
    }
    const bool alreadyPending = ::sigismember(&pending, SIGPIPE) == 1;
    const bool written = writeAll(STDERR_FILENO, record);
    const int error = errno;
    if (!written && error == EPIPE && !alreadyPending && ::sigpending(&pending) == 0 &&
        ::sigismember(&pending, SIGPIPE) == 1) {
        // A failed pipe write raised this thread's SIGPIPE. Consume only this
        // new pending signal before restoring a possibly unblocked mask.
        int received;
        int result;
        do {
            result = ::sigwait(&blocked, &received);
        } while (result == EINTR);
    }
    ::pthread_sigmask(SIG_SETMASK, &previous, nullptr);
}

void reportDiagnosticFileFailure(const char* operation, int error) noexcept {
    static std::atomic<long long> nextReport{0};
    static std::atomic<unsigned long long> suppressed{0};
    timespec time{};
    if (::clock_gettime(CLOCK_MONOTONIC, &time) != 0)
        time.tv_sec = 1;
    auto next = nextReport.load(std::memory_order_relaxed);
    if (time.tv_sec < next || !nextReport.compare_exchange_strong(next, time.tv_sec + 60)) {
        suppressed.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const auto dropped = suppressed.exchange(0, std::memory_order_relaxed);
    char message[256];
    const int length = std::snprintf(message, sizeof(message),
                                     "[error] diagnostic file failure operation=%.32s errno=%d suppressed=%llu\n",
                                     operation, error, dropped);
    if (length > 0 && static_cast<std::size_t>(length) < sizeof(message))
        detail::writeDiagnosticStderr(std::string_view(message, static_cast<std::size_t>(length)));
}

namespace {
bool appendFile(const char* path, std::string_view record, DiagnosticFileLimits limits, bool bounded) noexcept {
    try {
        constexpr std::string_view truncated = " [truncated]\n";
        if (!path || !*path ||
            (bounded && (limits.bytes < truncated.size() || limits.backups == 0 || limits.backups > 32)))
            return fail("invalid arguments", EINVAL);
        std::string limited;
        if (bounded && record.size() > limits.bytes) {
            limited.assign(record.substr(0, limits.bytes - truncated.size()));
            limited += truncated;
            record = limited;
        }
        const std::string name(path);
        Descriptor lock(openFile((name + ".lock").c_str(), O_WRONLY | O_CREAT));
        if (lock.value < 0)
            return fail("open lock", errno);
        int result;
        do {
            result = ::flock(lock.value, LOCK_EX);
        } while (result < 0 && errno == EINTR);
        if (result != 0)
            return fail("lock", errno);

        struct stat status {};
        do {
            result = ::stat(path, &status);
        } while (result != 0 && errno == EINTR);
        if (result != 0 && errno != ENOENT)
            return fail("stat", errno);
        if (bounded && status.st_size > 0 &&
            (static_cast<unsigned long long>(status.st_size) > limits.bytes - record.size())) {
            for (unsigned index = limits.backups; index > 1; --index) {
                if (!renameExisting(name + "." + std::to_string(index - 1), name + "." + std::to_string(index)))
                    return fail("rotate backup", errno);
            }
            if (!renameExisting(name, name + ".1"))
                return fail("rotate current", errno);
        }
        Descriptor output(openFile(path, O_WRONLY | O_CREAT | O_APPEND));
        if (output.value < 0)
            return fail("open record", errno);
        if (!writeAll(output.value, record))
            return fail("write record", errno);
        const int descriptor = output.value;
        output.value = -1;
        if (::close(descriptor) != 0)
            return fail("close record", errno);
        return true;
    } catch (const std::bad_alloc&) {
        return fail("allocation", ENOMEM);
    } catch (...) {
        return fail("format record", EINVAL);
    }
}
} // namespace

bool appendBoundedDiagnosticFile(const char* path, std::string_view record, DiagnosticFileLimits limits) noexcept {
    return appendFile(path, record, limits, true);
}

bool appendUnrotatedLogFile(const char* path, std::string_view record) noexcept {
    return appendFile(path, record, {}, false);
}
} // namespace de
