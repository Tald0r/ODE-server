#include "ServerApplication.h"

#include <ostream>
#include <utility>

#include "Exception.h"
#include "KernelContext.h"
#include "ServerPortSettings.h"
#include "ServerStartup.h"

namespace de {

ServerApplication::ServerApplication(KernelContext& context) noexcept : m_Context(context) {}

ServerApplication::~ServerApplication() {
    if (m_Config)
        m_Context.setConfig(m_PreviousConfig);
}

std::optional<ServerLifecycleResult> ServerApplication::run(ServerKind server, int argc, const char* const argv[],
                                                            const ServerLifecycleActions& actions, std::ostream& output,
                                                            std::ostream& errors, const std::string& instantLogPath) {
    if (m_HasRun)
        throw Error("ServerApplication can only run once");
    m_HasRun = true;

    try {
        const auto options = parseServerOptions(server, argc, argv);
        if (server == ServerKind::Game)
            output << ">>> COMMAND-LINE PARAMETER READING SUCCESS..." << std::endl;
        auto config = loadServerConfiguration(options);
        validateServerListenerPorts(server, *config);
        m_Config = std::move(config);
        m_PreviousConfig = m_Context.exchangeConfig(m_Config.get());
        if (server != ServerKind::Game)
            output << m_Config->toString() << std::endl;
        if (options.loginIDOffset) {
            output << "LoginServerPort : " << m_Config->getProperty("LoginServerPort") << std::endl;
            output << "LoginServerUDPPort : " << m_Config->getProperty("LoginServerUDPPort") << std::endl;
            output << "LoginServerID : " << m_Config->getProperty("LoginServerID") << std::endl;
        }
    } catch (const Throwable& error) {
        errors << error.toString() << std::endl;
        return std::nullopt;
    }

    const auto result = runServerLifecycle(actions, output, errors, instantLogPath);
    if (result.drained) {
        switch (server) {
        case ServerKind::Game:
            output << ">>> ALL GAME WORKERS STOPPED." << std::endl;
            break;
        case ServerKind::Login:
            output << ">>> ALL LOGIN WORKERS STOPPED." << std::endl;
            break;
        case ServerKind::Shared:
            output << ">>> ALL SHARED WORKERS STOPPED." << std::endl;
            break;
        }
    }
    output.flush();
    errors.flush();
    return result;
}

} // namespace de
