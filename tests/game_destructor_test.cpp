//////////////////////////////////////////////////////////////////////
//
// Filename    : game_destructor_test.cpp
// Description : What gameserver destructors do where their cleanup
//               can fail.
//
//               A destructor cannot let an exception out, so each case
//               drives the failure and pins what the destructor does
//               instead: it takes no lock of its own object, so the
//               thread deleting it may already hold that lock, and a
//               broken invariant is logged the way a failed Assert logs
//               it, work that fails is logged to destructorError.log, and
//               the destructor carries on.
//
//////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "EffectAddItem.h"
#include "EffectSchedule.h"
#include "GCReconnectLogin.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "HolyLandManager.h"
#include "InventorySlot.h"
#include "Party.h"
#include "Socket.h"
#include "item/Money.h"

namespace {

// Collects what std::cerr receives while it lives; a failed lock is
// reported there before Mutex::lock throws.
class CerrCapture {
public:
    CerrCapture() : m_Previous(std::cerr.rdbuf(m_Buffer.rdbuf())) {}
    ~CerrCapture() {
        std::cerr.rdbuf(m_Previous);
    }
    std::string text() const {
        return m_Buffer.str();
    }

private:
    std::ostringstream m_Buffer;
    std::streambuf* m_Previous;
};

std::string readFile(const char* name) {
    std::ifstream in(name);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

class CountingGuild : public Guild {
public:
    explicit CountingGuild(int& destroyed) : m_Destroyed(destroyed) {}
    ~CountingGuild() override {
        ++m_Destroyed;
    }

private:
    int& m_Destroyed;
};

// The manager's mutex is error-checking: had the destructor locked it, the
// lock would fail on this thread and the guilds would never be freed. The
// mutex dies still locked, which Mutex::~Mutex reports and survives.
TEST(GameDestructor, AGuildManagerDeletedUnderItsOwnLockFreesItsGuilds) {
    int destroyed = 0;
    GuildManager* pManager = new GuildManager();
    Guild* pGuild = new CountingGuild(destroyed);
    pGuild->setID(1);
    pManager->addGuild(pGuild);
    pManager->lock();

    CerrCapture cerr;
    delete pManager;

    EXPECT_EQ(destroyed, 1);
    EXPECT_EQ(cerr.text().find("Mutex::lock"), std::string::npos) << cerr.text();
}

// Guild and Party keep their mutex protected: a subclass's destructor body
// locks it, so ~Guild and ~Party run with the lock held, the way they would
// for a deleting thread that held it.
class LockedGuild : public Guild {
public:
    ~LockedGuild() override {
        m_Mutex.lock();
    }
};

class LockedParty : public Party {
public:
    LockedParty() : Party(Creature::CREATURE_CLASS_SLAYER) {}
    ~LockedParty() {
        m_Mutex.lock();
    }
};

TEST(GameDestructor, AGuildDestroyedUnderItsOwnLockTakesNoLock) {
    LockedGuild* pGuild = new LockedGuild();
    GuildMember* pMember = new GuildMember();
    pMember->setName("Reiot");
    pGuild->addMember(pMember);

    CerrCapture cerr;
    delete pGuild;

    EXPECT_EQ(cerr.text().find("Mutex::lock"), std::string::npos) << cerr.text();
}

TEST(GameDestructor, APartyDestroyedUnderItsOwnLockTakesNoLock) {
    LockedParty* pParty = new LockedParty();

    CerrCapture cerr;
    delete pParty;

    EXPECT_EQ(cerr.text().find("Mutex::lock"), std::string::npos) << cerr.text();
}

TEST(GameDestructor, AHolyLandManagerDeletedUnderItsOwnLockTakesNoLock) {
    HolyLandManager* pManager = new HolyLandManager();
    pManager->lock();

    CerrCapture cerr;
    delete pManager;

    EXPECT_EQ(cerr.text().find("Mutex::lock"), std::string::npos) << cerr.text();
}

TEST(GameDestructor, AnInventorySlotDestroyedHoldingAnItemLogsTheAssertion) {
    std::remove("assertion_failed.log");
    Money money;
    {
        InventorySlot slot;
        slot.addItem(&money);
    }
    EXPECT_NE(readFile("assertion_failed.log").find("m_pItem == NULL"), std::string::npos);
}

TEST(GameDestructor, AnEffectScheduleWorkDestroyedHoldingDataLogsTheAssertion) {
    std::remove("assertion_failed.log");
    int data = 0;
    {
        EffectScheduleWork work;
        work.setCode(WORKCODE_DELETE);
        work.setData(&data);
    }
    EXPECT_NE(readFile("assertion_failed.log").find("m_pData == NULL"), std::string::npos);
}

// GamePlayer frees its reconnect packet at the end of its destructor, so
// this packet shows whether the teardown got that far.
class CountingReconnect : public GCReconnectLogin {
public:
    explicit CountingReconnect(int& destroyed) : m_Destroyed(destroyed) {}
    ~CountingReconnect() override {
        ++m_Destroyed;
    }

private:
    int& m_Destroyed;
};

TEST(GameDestructor, AGamePlayerDestroyedBeforeItsSessionEndedLogsTheAssertionAndFinishesItsTeardown) {
    std::remove("assertion_failed.log");
    int destroyed = 0;
    GamePlayer* pPlayer = new GamePlayer(new Socket());
    pPlayer->setPlayerStatus(GPS_BEGIN_SESSION);
    pPlayer->setReconnectPacket(new CountingReconnect(destroyed));

    delete pPlayer;

    EXPECT_EQ(destroyed, 1);
    EXPECT_NE(readFile("assertion_failed.log").find("m_PlayerStatus == GPS_END_SESSION"), std::string::npos);
}

// The effect only stores its zone; with no target, unaffect() fails its
// first Assert before it would touch the zone.
char g_zoneStandIn[1];

TEST(GameDestructor, AnEffectAddItemDestroyedWithoutATargetLogsTheFailure) {
    std::remove("destructorError.log");
    Money money;
    money.setObjectID(1);
    {
        EffectAddItem effect(reinterpret_cast<Zone*>(g_zoneStandIn), 0, 0, &money, 0, false);
        effect.setTarget(NULL);
    }
    EXPECT_NE(readFile("destructorError.log").find("~EffectAddItem: "), std::string::npos);
}

} // namespace
