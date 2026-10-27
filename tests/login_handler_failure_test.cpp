//////////////////////////////////////////////////////////////////////////////
// Filename    : login_handler_failure_test.cpp
// Description : What a login handler's throw does to its session and to the
//               manager's loop. A ProtocolException (a logout's
//               DisconnectException) and a ConnectException are the handlers'
//               ways to end a session, and the manager retires the player for
//               each; anything else a handler throws ends the session the same
//               way instead of leaving processCommands for the main loop.
//
//               The packet arrives over a real loopback connection, through a
//               factory, validator and handler registered in the child only.
//               The handler queues a reply before it throws, so the peer
//               shows how the session was retired: a session that ends
//               flushes the reply, and one whose connection is lost does not.
//////////////////////////////////////////////////////////////////////////////

#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <sys/socket.h>

#include "Exception.h"
#include "KernelContext.h"
#include "LCVersionCheckOK.h"
#include "LoginPlayerManager.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "Player.h"
#include "Socket.h"
#include "SocketOutputStream.h"
#include "support/LoopbackListener.h"

namespace {

std::exception_ptr g_failure;
unsigned g_calls = 0;

void replyAndThrow(Packet* pPacket, Player* pPlayer) {
    ++g_calls;
    pPlayer->sendPacket(pPacket);
    std::rethrow_exception(g_failure);
}

bool readable(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & POLLIN) != 0;
}

// Everything the peer receives until the connection closes, or what came
// within two seconds.
std::string receiveUntilClosed(int peer) {
    std::string received;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    char buffer[256];
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd ready{peer, POLLIN, 0};
        if (::poll(&ready, 1, 10) != 1)
            continue;
        const auto count = ::recv(peer, buffer, sizeof(buffer), 0);
        if (count <= 0)
            break;
        received.append(buffer, count);
    }
    return received;
}

// Delivers one packet whose handler queues a reply and throws failure, and
// exits 0 when the handler ran once, processCommands returned with the
// player retired, and the peer received the reply exactly when flushed.
void checkHandlerFailureRetiresThePlayer(std::exception_ptr failure, bool flushed) {
    g_failure = failure;
    PacketFactoryManager factories;
    factories.addFactory(new LCVersionCheckOKFactory());
    PacketValidator validator;
    auto allowed = std::make_unique<PacketIDSet>(LPS_BEGIN_SESSION);
    allowed->addPacketID(Packet::PACKET_LC_VERSION_CHECK_OK);
    validator.addPacketIDSet(LPS_BEGIN_SESSION, allowed.release());
    de::kernelContext().setPacketFactoryManager(&factories);
    de::kernelContext().setPacketValidator(&validator);
    PacketDispatcher::registerHandler(Packet::PACKET_LC_VERSION_CHECK_OK, replyAndThrow);

    LoopbackListener listener;
    LoginPlayerManager manager;
    auto incoming = std::make_unique<Socket>("127.0.0.1", listener.port());
    incoming->connect();
    const int peer = listener.accept();
    const int descriptor = incoming->getSOCKET();
    manager.acceptNewConnection(incoming.release());

    LCVersionCheckOK packet;
    SocketOutputStream wire(nullptr, 64);
    wire.writePacket(&packet);
    if (::send(peer, wire.getBuffer(), wire.length(), 0) != wire.length() || !readable(descriptor))
        std::_Exit(1);
    manager.pollSockets();
    manager.processInputs();
    try {
        manager.processCommands();
    } catch (...) {
        std::_Exit(2);
    }
    if (g_calls != 1 || manager.size() != 0)
        std::_Exit(3);
    const std::string expected = flushed ? std::string(wire.getBuffer(), wire.length()) : std::string();
    if (receiveUntilClosed(peer) != expected)
        std::_Exit(4);
    ::close(peer);
    std::_Exit(0);
}

TEST(LoginHandlerFailure, AnyThrowFromAHandlerRetiresOnlyItsPlayer) {
    const struct {
        const char* name;
        std::exception_ptr failure;
        bool flushed;
    } cases[] = {
        {"RuntimeException", std::make_exception_ptr(RuntimeException("handler failed")), true},
        {"std::runtime_error", std::make_exception_ptr(std::runtime_error("handler failed")), true},
        {"DisconnectException", std::make_exception_ptr(DisconnectException("logout")), true},
        {"ConnectException", std::make_exception_ptr(ConnectException("send failed")), false},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        EXPECT_EXIT(checkHandlerFailureRetiresThePlayer(c.failure, c.flushed), ::testing::ExitedWithCode(0), "");
    }
}

} // namespace
