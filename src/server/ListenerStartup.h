#ifndef DARKEDEN_LISTENER_STARTUP_H
#define DARKEDEN_LISTENER_STARTUP_H

#include <chrono>
#include <functional>
#include <string>

class BindException;

namespace de {

// Retry only BindException, reporting each failure before the next attempt.
// The bind action must release resources from a failed attempt and publish its
// listener only once ready. Other action/reporting exceptions propagate.
// An existing shutdown request prevents the first attempt; a later request
// interrupts the retry wait. Shutdown throws Error with the transport label.
// Does not reset shutdown state or retry a successful attempt.
void retryListenerStartup(const std::function<void()>& bind,
                          const std::function<void(const BindException&)>& reportFailure, const std::string& transport,
                          std::chrono::microseconds retryDelay = std::chrono::seconds(1));

} // namespace de

#endif
