//////////////////////////////////////////////////////////////////////////////
// Filename    : game_level_war_command_test.cpp
// Description : *command LevelWar from a thread that does not own the level
//               war zone. The manager's schedule queue is the zone group's
//               state, run by that group's heartbeat, so the command posts
//               the start to the group and the queue changes only when the
//               group drains its mailbox under its own mutex.
//
//               The zone, its group and its info are built in memory with no
//               database; the context they are registered in is process-wide,
//               so the case runs in its own process.
//////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <thread>

#include <gtest/gtest.h>

#include "CastleInfoManager.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "LevelWarManager.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "gm/ConsoleSubcommands.h"

class ZoneTestAccess {
public:
    static void setLevelWarManager(Zone& zone, LevelWarManager* pManager) {
        zone.m_pLevelWarManager = pManager;
    }
};

namespace {

void checkLevelWarStartIsPostedToTheZoneGroup() {
    VariableManager variables;
    variables.setVariable(WAR_ACTIVE, 1);
    variables.setVariable(ACTIVE_LEVEL_WAR, 1);
    CastleInfoManager castles;
    ZoneInfoManager zoneInfos;
    ZoneGroupManager zoneGroups;
    de::gameContext().setVariableManager(&variables);
    de::gameContext().setCastleInfoManager(&castles);
    de::gameContext().setZoneInfoManager(&zoneInfos);
    de::gameContext().setZoneGroupManager(&zoneGroups);

    ZoneInfo* pInfo = new ZoneInfo();
    pInfo->setZoneID(1131);
    pInfo->setZoneGroupID(1);
    zoneInfos.addZoneInfo(pInfo);
    ZoneGroup* pGroup = new ZoneGroup(1);
    zoneGroups.addZoneGroup(pGroup);
    Zone* pZone = new Zone(1131);
    pZone->setZoneGroup(pGroup);
    pGroup->addZone(pZone);
    LevelWarManager* pManager = new LevelWarManager(1, pZone);
    ZoneTestAccess::setLevelWarManager(*pZone, pManager);

    // This thread stands in for the zone's: it holds the group as a tick does.
    pGroup->armOwnershipAssert();
    pGroup->lock();
    std::thread gm([] {
        GCSystemMessage message;
        bool send = false;
        de::gm::opLevelWar(NULL, "1131", message, send);
    });
    gm.join();
    if (pManager->getSize() != 0)
        std::_Exit(1);

    if (pGroup->drainMailbox() != 1 || pManager->getSize() != 1)
        std::_Exit(2);
    pGroup->unlock();
    std::_Exit(0);
}

TEST(GameLevelWarCommand, StartFromAnotherThreadIsPostedToTheZoneGroup) {
    ASSERT_EXIT(checkLevelWarStartIsPostedToTheZoneGroup(), ::testing::ExitedWithCode(0), "");
}

} // namespace
