#include "ListenerStartup.h"

#include <algorithm>
#include <thread>

#include "Exception.h"
#include "ServerShutdown.h"

namespace de {

void retryListenerStartup(const std::function<void()>& bind,
                          const std::function<void(const BindException&)>& reportFailure, const std::string& transport,
                          std::chrono::microseconds retryDelay) {
    while (!ServerShutdown::isRequested()) {
        try {
            bind();
            return;
        } catch (const BindException& error) {
            reportFailure(error);
        }

        const auto started = std::chrono::steady_clock::now();
        while (!ServerShutdown::isRequested()) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
            if (elapsed >= retryDelay)
                break;
            std::this_thread::sleep_for(std::min(retryDelay - elapsed, std::chrono::microseconds(10000)));
        }
    }
    throw Error("shutdown requested during " + transport + " listener startup");
}

} // namespace de
