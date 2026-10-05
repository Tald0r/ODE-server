#ifndef DARKEDEN_REPEATED_ERROR_REPORT_H
#define DARKEDEN_REPEATED_ERROR_REPORT_H

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>

namespace de {

// One fixed failure category, owned by its worker. Callers synchronize shared
// instances. A successful operation does not replenish the reporting budget.
class RepeatedErrorReport {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    static constexpr auto interval = std::chrono::seconds(30);

    std::optional<std::uint64_t> failure(TimePoint now) noexcept {
        if (!lastReport || now - *lastReport >= interval) {
            lastReport = now;
            reportedFailure = true;
            const auto count = suppressed;
            suppressed = 0;
            return count;
        }
        if (suppressed != std::numeric_limits<std::uint64_t>::max())
            ++suppressed;
        return std::nullopt;
    }

    // Report recovery only for an emitted failure. Alternating success/failure
    // cannot produce more recovery messages than failure messages.
    std::optional<std::uint64_t> recovery() noexcept {
        if (!reportedFailure)
            return std::nullopt;
        reportedFailure = false;
        return takeSuppressed();
    }

    std::uint64_t takeSuppressed() noexcept {
        const auto count = suppressed;
        suppressed = 0;
        return count;
    }

private:
    std::optional<TimePoint> lastReport;
    std::uint64_t suppressed = 0;
    bool reportedFailure = false;
};

} // namespace de

#endif
