#ifndef DARKEDEN_LISTENER_STARTUP_H
#define DARKEDEN_LISTENER_STARTUP_H

#include <chrono>
#include <functional>
#include <string>

#include "RepeatedErrorReport.h"

class BindException;

namespace de {

enum class ListenerStartupReport { RepeatedFailure, Recovered, Stopped };

// Retry only BindException. Report the first failure immediately, then at most
// once every 30 seconds with the number suppressed. Recovery and shutdown flush
// remaining counts. Clock injection affects reporting only, never retry timing.
// The bind action must release resources from a failed attempt and publish its
// listener only once ready. Binding/failure-action exceptions propagate;
// optional summary failures cannot change binding or shutdown outcomes.
// An existing shutdown request prevents the first attempt; a later request
// interrupts the retry wait. Shutdown throws Error with the transport label.
// Does not reset shutdown state or retry a successful attempt.
void retryListenerStartup(const std::function<void()>& bind,
                          const std::function<void(const BindException&)>& reportFailure, const std::string& transport,
                          std::chrono::microseconds retryDelay = std::chrono::seconds(1),
                          const std::function<RepeatedErrorReport::TimePoint()>& now = RepeatedErrorReport::Clock::now,
                          const std::function<void(ListenerStartupReport, std::uint64_t)>& reportSummary = {});

} // namespace de

#endif
