#ifndef DARKEDEN_SERVER_APPLICATION_H
#define DARKEDEN_SERVER_APPLICATION_H

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>

#include "ServerKind.h"
#include "ServerLifecycle.h"

class Properties;

namespace de {

class KernelContext;

// Own the configuration and its binding for one application run on the main
// thread. Keep this object alive while any server/worker can use that binding.
// Destruction restores the previous binding before releasing the configuration.
// Production keeps the object alive through _Exit, including failed drains.
class ServerApplication {
public:
    explicit ServerApplication(KernelContext& context) noexcept;
    ~ServerApplication();

    ServerApplication(const ServerApplication&) = delete;
    ServerApplication& operator=(const ServerApplication&) = delete;

    // Load, apply offsets and validate required listener ports before publishing
    // the configuration or invoking any action.
    // An empty result means configuration failed; no lifecycle action has run.
    // Otherwise return the lifecycle result after reporting and flushing its
    // diagnostics. Does not install process handlers, exit or destroy servers.
    // A second call is a programming error and throws Error.
    [[nodiscard]] std::optional<ServerLifecycleResult> run(ServerKind server, int argc, const char* const argv[],
                                                           const ServerLifecycleActions& actions, std::ostream& output,
                                                           std::ostream& errors,
                                                           const std::string& instantLogPath = "../log/instant.log");

private:
    KernelContext& m_Context;
    std::unique_ptr<Properties> m_Config;
    Properties* m_PreviousConfig = nullptr;
    bool m_HasRun = false;
};

} // namespace de

#endif
