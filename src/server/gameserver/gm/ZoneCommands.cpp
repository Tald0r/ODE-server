//////////////////////////////////////////////////////////////////////////////
// Filename    : ZoneCommands.cpp
// Description : GM commands that act on a zone or on what stands in it: warps, recalls, summons
//               and the zone flags.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "ItemUtil.h"
#include "Monster.h"
#include "OptionInfo.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "RelicUtil.h"
#include "SiegeManager.h"
#include "VSDateTime.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"

namespace de::gm {
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opzone(string msg, int i) {
    __BEGIN_TRY

    //////////////
    // Zone Info
    //////////////
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opwarp(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();

    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1);
    size_t l = msg.find_first_of(' ', k + 1);

    string ZoneName = trim(msg.substr(j + 1, k - j - 1));
    ZoneCoord_t ZoneX = atoi(msg.substr(k + 1, l - k - 1).c_str());
    ZoneCoord_t ZoneY = atoi(msg.substr(l + 1, msg.size() - l - 1).c_str());

    int ZoneID = 0;

    // ZoneName can hold a zone name or a zone ID.
    // A non-NULL ZoneInfo means the string was a zone name,
    // and a NULL ZoneInfo means the string was a zone ID.
    // (a typo is possible, but that is ignored.)
    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfoByName(ZoneName);
    if (pZoneInfo != NULL) {
        ZoneID = pZoneInfo->getZoneID();
    } else {
        ZoneID = atoi(ZoneName.c_str());
    }

    if (pCreature->isPC() && ZoneX < 256 && ZoneY < 256) {
        // Check that such a zone really exists.
        // Return if there is none.
        try {
            getZoneByZoneID(ZoneID);
        } catch (Error) {
            return;
        }

        // Once the zone is confirmed to exist, move there.
        try {
            transportCreature(pCreature, ZoneID, ZoneX, ZoneY, false);
        } catch (Throwable& t) {
            cerr << t.toString() << endl;
        }
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void oprecall(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    if (pPC->isPLAYER()) {
        if (de::gameContext().guilds().isGuildMaster(pPC->getGuildID(), pPC)) {
            if (!SiegeManager::Instance().isSiegeZone(pPC->getZoneID())) {
                return;
            }
        } else {
            return;
        }
    }


    Creature* pTCreature = NULL;

    size_t j = msg.find_first_of(' ', i + 1);

    string Name;

    while (j < msg.size()) {
        i = msg.find_first_of(' ', j + 1);

        Name = msg.substr(j + 1, i - j - 1).c_str();


        // NoSuch removed.
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        pTCreature = pcFinder.getCreature_LOCKED(Name);

        if (pTCreature == NULL) {
            return;
        }

        {
            // Careful: the Creature found through the PCFinder is const.
            Zone* pTargetZone = pTCreature->getZone();

            Assert(pTargetZone != NULL);

            Creature* pTargetCreature = NULL;
            // try
            //  NoSuch removed.
            pTargetCreature = pTargetZone->getCreature(pTCreature->getObjectID());

            if (pTargetCreature != NULL) {
                // A creature that is currently dead cannot be moved.
                if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_COMA)) {
                    return;
                }

                // The coordinates to summon to.
                ZoneID_t ZoneNum = pCreature->getZoneID();
                Coord_t ZoneX = pCreature->getX();
                Coord_t ZoneY = pCreature->getY();

                if (SiegeManager::Instance().isSiegeZone(pPC->getZoneID())) {
                    Effect::EffectClass eClass = Effect::EFFECT_CLASS_MAX;
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_DEFENDER)) {
                        ZoneX = 172;
                        ZoneY = 38;
                        eClass = Effect::EFFECT_CLASS_SIEGE_DEFENDER;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_REINFORCE)) {
                        ZoneX = 172;
                        ZoneY = 38;
                        eClass = Effect::EFFECT_CLASS_SIEGE_REINFORCE;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_1)) {
                        ZoneX = 20;
                        ZoneY = 232;
                        eClass = Effect::EFFECT_CLASS_SIEGE_ATTACKER_1;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_2)) {
                        ZoneX = 20;
                        ZoneY = 232;
                        eClass = Effect::EFFECT_CLASS_SIEGE_ATTACKER_2;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_3)) {
                        ZoneX = 20;
                        ZoneY = 232;
                        eClass = Effect::EFFECT_CLASS_SIEGE_ATTACKER_3;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_4)) {
                        ZoneX = 20;
                        ZoneY = 232;
                        eClass = Effect::EFFECT_CLASS_SIEGE_ATTACKER_4;
                    } else if (pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_ATTACKER_5)) {
                        ZoneX = 20;
                        ZoneY = 232;
                        eClass = Effect::EFFECT_CLASS_SIEGE_ATTACKER_5;
                    }

                    if (eClass != Effect::EFFECT_CLASS_MAX && !pTargetCreature->isFlag(eClass)) {
                        if (!pTargetCreature->isPC() ||
                            dynamic_cast<PlayerCreature*>(pTargetCreature)->getGuildID() != pPC->getGuildID()) {
                            return;
                        }

                        addSimpleCreatureEffect(pTargetCreature, eClass, 72000);
                    }
                }

                Assert((int)ZoneX < 256);
                Assert((int)ZoneY < 256);

                Assert(pTargetCreature->isPC());

                transportCreature(pTargetCreature, ZoneNum, ZoneX, ZoneY, false);
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)

        j = i;
    }

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opsummon(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();
    MonsterInfoManager& monsterInfos = de::gameContext().monsterInfos();

    size_t j = msg.find_first_of(' ', i + 1);
    size_t k = msg.find_first_of(' ', j + 1); // j~k : name
    size_t l = msg.find_first_of(' ', k + 1); // k~l : num
    size_t m = msg.find_first_of(' ', l + 1); // l~m : mad

    size_t o = msg.find_first_of('{', j);
    size_t p = msg.find_first_of('}', j + 1);


    SpriteType_t SpriteType = 0;
    MonsterType_t MonsterType = 0;
    int MonsterNum = 0;

    string MonsterName = msg.substr(j + 1, k - j - 1);


    // When a MonsterType is given instead of a SpriteType
    if (o != string::npos && p != string::npos) {
        MonsterType = atoi(msg.substr(o + 1, p - o - 1).c_str());
    } else if (MonsterName.rfind("Chief", 0) == 0 || strstr(MonsterName.c_str(), "\xec\xb9\x98\xed\x94\x84") != NULL) {
        // Summoning a chief monster: every chief monster's HName in
        // MonsterInfo starts with "Chief" (the English seed) or with the
        // Korean word for it, whose UTF-8 bytes the second test spells.
        MonsterType = monsterInfos.getChiefMonsterTypeByName(MonsterName);
    } else {
        SpriteType = monsterInfos.getSpriteTypeByName(MonsterName);

        if (SpriteType == 0) {
            SpriteType = atoi(msg.substr(j + 1, k - j - 1).c_str());
        }
    }

    MonsterNum = atoi(msg.substr(k + 1, msg.size() - k - 1).c_str());

    // clamp to 1~30
    MonsterNum = max(1, MonsterNum);
    MonsterNum = min(30, MonsterNum);

    // ClanType
    // "mad" monsters -_-;
    string mad = trim(msg.substr(l + 1, m - l - 1));

    SUMMON_INFO summonInfo;
    if (mad == "mad") {
        summonInfo.canScanEnemy = true;

        string group = trim(msg.substr(m + 1, msg.size() - m - 1));

        if (group == "group") {
            summonInfo.clanType = SUMMON_INFO::CLAN_TYPE_RANDOM_GROUP;
            summonInfo.clanID = rand() % 90 + 2;
        } else {
            summonInfo.clanType = SUMMON_INFO::CLAN_TYPE_RANDOM_EACH;
        }
    } else {
        summonInfo.canScanEnemy = false;
        summonInfo.clanType = SUMMON_INFO::CLAN_TYPE_DEFAULT;
    }

    Zone* pZone = pCreature->getZone();
    Coord_t ZoneX = pCreature->getX();
    Coord_t ZoneY = pCreature->getY();

    try {
        // Summon the monster when it is an ordinary zone.
        if (!(pZone->getZoneLevel() & SAFE_ZONE)) {
            filelog("summon.txt", "[%s] ZoneID=%d, %s", pCreature->getName().c_str(), pCreature->getZone()->getZoneID(),
                    msg.c_str());

            // To check whether the monsterInfo exists or not..
            // It throws a NoSuchElementException when there is none.
            if (SpriteType != 0) {
                monsterInfos.getMonsterTypeBySprite(SpriteType);
                addMonstersToZone(pZone, ZoneX, ZoneY, SpriteType, MonsterType, MonsterNum, summonInfo);
            } else if (MonsterType != 0) {
                monsterInfos.getMonsterInfo(MonsterType);
                addMonstersToZone(pZone, ZoneX, ZoneY, SpriteType, MonsterType, MonsterNum, summonInfo);
            }
        }
    } catch (Throwable& t) {
        cout << t.toString() << endl;
        filelog("summon.txt", "[%s]%s",
                (pGamePlayer == NULL ? "Nobody" : pGamePlayer->getCreature()->getName().c_str()), t.toString().c_str());
    }


    __END_DEBUG_EX __END_CATCH
}

void opopenpaymap(GamePlayer* pGamePlayer, string msg, int i) {
    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(1013);
    pZoneInfo->setNoPortalZone(true);
    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage("Pay zone opened");
    pGamePlayer->sendPacket(&gcSystemMessage);
}

void opclosepaymap(GamePlayer* pGamePlayer, string msg, int i) {
    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(1013);
    pZoneInfo->setNoPortalZone(false);
    GCSystemMessage gcSystemMessage1;
    gcSystemMessage1.setMessage("Pay zone closed");
    pGamePlayer->sendPacket(&gcSystemMessage1);
}
} // namespace de::gm
