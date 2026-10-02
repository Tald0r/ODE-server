#include <mutex>

#include <gtest/gtest.h>

#include "CLLogin.h"
#include "GCAddStoreItem.h"
#include "GCCannotUse.h"
#include "GCRemoveStoreItem.h"
#include "GSRequestGuildInfo.h"
#include "GamePacketDispatch.h"
#include "PacketDispatcher.h"

#if !defined(__GAME_SERVER__) || !defined(__COMBAT__) || defined(__LOGIN_SERVER__) || defined(__SHARED_SERVER__)
#error "GameServerRuntime must supply only the gameserver's compile definitions"
#endif

namespace {

class GameServerRuntimeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Registration is process-wide, including when gtest repeats a suite.
        static std::once_flag registration;
        std::call_once(registration, registerGameServerPacketHandlers);
    }
};

// The client sends these legacy packets to the gameserver. Its production
// registration deliberately ignores them instead of disconnecting the client.
TEST_F(GameServerRuntimeTest, AcceptsLegacyAddStoreItem) {
    GCAddStoreItem packet;
    EXPECT_NO_THROW(PacketDispatcher::dispatch(&packet, nullptr));
}

TEST_F(GameServerRuntimeTest, AcceptsLegacyRemoveStoreItem) {
    GCRemoveStoreItem packet;
    EXPECT_NO_THROW(PacketDispatcher::dispatch(&packet, nullptr));
}

TEST_F(GameServerRuntimeTest, AcceptsLegacyCannotUse) {
    GCCannotUse packet;
    EXPECT_NO_THROW(PacketDispatcher::dispatch(&packet, nullptr));
}

TEST_F(GameServerRuntimeTest, RejectsLoginRequests) {
    CLLogin packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

TEST_F(GameServerRuntimeTest, RejectsSharedServerRequests) {
    GSRequestGuildInfo packet;
    EXPECT_THROW(PacketDispatcher::dispatch(&packet, nullptr), InvalidProtocolException);
}

} // namespace
