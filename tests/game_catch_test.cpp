//////////////////////////////////////////////////////////////////////
//
// Filename    : game_catch_test.cpp
// Description : What gameserver code outside destructors does with a
//               failure no caller handles: it logs it where it is
//               caught and carries on.
//
//////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "GCReconnectLogin.h"
#include "GamePlayer.h"
#include "Socket.h"

namespace {

std::string readFile(const char* name) {
    std::ifstream in(name);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// A never-connected socket cannot send: the reconnect packet is written to
// the output buffer, both of disconnect()'s flushes fail on it, and the
// session still ends.
TEST(GameCatch, ADisconnectThatCannotFlushLogsEachFailureAndEndsTheSession) {
    std::remove("GamePlayer.txt");
    GamePlayer* pPlayer = new GamePlayer(new Socket());
    pPlayer->setPlayerStatus(GPS_NORMAL);
    GCReconnectLogin* pReconnect = new GCReconnectLogin();
    pReconnect->setLoginServerIP("127.0.0.1");
    pReconnect->setLoginServerPort(9999);
    pPlayer->setReconnectPacket(pReconnect);

    pPlayer->disconnect(UNDISCONNECTED);

    EXPECT_EQ(pPlayer->getPlayerStatus(), GPS_END_SESSION);
    const std::string log = readFile("GamePlayer.txt");
    EXPECT_NE(log.find("disconnect could not send the reconnect packet: "), std::string::npos) << log;
    EXPECT_NE(log.find("disconnect could not flush the output: "), std::string::npos) << log;
    delete pPlayer;
}

} // namespace
