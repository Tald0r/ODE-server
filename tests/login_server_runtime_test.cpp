#include <mutex>

#include <gtest/gtest.h>

#include "CLLogout.h"
#include "GCAddStoreItem.h"
#include "GSRequestGuildInfo.h"
#include "LoginPacketDispatch.h"
#include "PacketDispatcher.h"
#include "Player.h"
#include "support/ConnectionKeyDispatchChecks.h"

#if !defined(__LOGIN_SERVER__) || defined(__GAME_SERVER__) || defined(__SHARED_SERVER__)
#error "LoginServerRuntime must supply only the loginserver's compile definitions"
#endif

namespace {

class LoginServerRuntimeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static std::once_flag registration;
        std::call_once(registration, registerLoginServerPacketHandlers);
    }
};

TEST_F(LoginServerRuntimeTest, LogoutRunsTheProductionHandler) {
    // The real logout handler requires a player, but no socket or account.
    // A macro-free recompile of the handler would silently do nothing.
    Player player;
    CLLogout packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, &player), DisconnectException);
}

TEST_F(LoginServerRuntimeTest, RejectsGameserverCompatibilityPackets) {
    GCAddStoreItem packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

TEST_F(LoginServerRuntimeTest, RejectsSharedServerRequests) {
    GSRequestGuildInfo packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

TEST_F(LoginServerRuntimeTest, ConnectionKeyDispatchReplacesOwnedStateWithoutExiting) {
    connection_key_test::checkDispatchedReplacement();
}

TEST_F(LoginServerRuntimeTest, ConnectionKeyDispatchPreservesStateOnAllocationFailureAndCanRetry) {
    connection_key_test::checkDispatchedFailureAndRetry();
}

} // namespace
