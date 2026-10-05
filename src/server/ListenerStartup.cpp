#include "ListenerStartup.h"

#include <algorithm>
#include <iostream>
#include <thread>

#include "Exception.h"
#include "ServerShutdown.h"

namespace de {

void retryListenerStartup(const std::function<void()>& bind,
                          const std::function<void(const BindException&)>& reportFailure, const std::string& transport,
                          std::chrono::microseconds retryDelay,
                          const std::function<RepeatedErrorReport::TimePoint()>& now,
                          const std::function<void(ListenerStartupReport, std::uint64_t)>& reportSummary) {
    RepeatedErrorReport failures;
    const auto summary = [&](ListenerStartupReport event, std::uint64_t suppressed) noexcept {
        // Optional aggregation must not undo a successfully published listener
        // or replace the shutdown result. The existing failure action retains
        // its exception contract.
        try {
            if (reportSummary) {
                reportSummary(event, suppressed);
                return;
            }
            const char* status = event == ListenerStartupReport::Recovered ? "recovered"
                                 : event == ListenerStartupReport::Stopped ? "stopped"
                                                                           : "retrying";
            std::ostream diagnostics(std::cerr.rdbuf());
            diagnostics << transport << " listener startup " << status << ": suppressed_failures=" << suppressed
                        << std::endl;
        } catch (...) {
        }
    };
    while (!ServerShutdown::isRequested()) {
        try {
            bind();
        } catch (const BindException& error) {
            if (const auto suppressed = failures.failure(now())) {
                reportFailure(error);
                if (*suppressed != 0)
                    summary(ListenerStartupReport::RepeatedFailure, *suppressed);
            }
            // Retry timing and shutdown behavior are independent of reporting.
            const auto started = std::chrono::steady_clock::now();
            while (!ServerShutdown::isRequested()) {
                const auto elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
                if (elapsed >= retryDelay)
                    break;
                std::this_thread::sleep_for(std::min(retryDelay - elapsed, std::chrono::microseconds(10000)));
            }
            continue;
        }
        if (const auto suppressed = failures.recovery())
            summary(ListenerStartupReport::Recovered, *suppressed);
        return;
    }
    if (const auto suppressed = failures.takeSuppressed(); suppressed != 0)
        summary(ListenerStartupReport::Stopped, suppressed);
    throw Error("shutdown requested during " + transport + " listener startup");
}

} // namespace de
