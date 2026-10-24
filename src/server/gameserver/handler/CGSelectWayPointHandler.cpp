//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectWayPointHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectWayPoint.h"

#ifdef __GAME_SERVER__
#include <cmath>

#include "CreatureUtil.h"
#include "DynamicZone.h"
#include "DynamicZoneGroup.h"
#include "DynamicZoneInfo.h"
#include "DynamicZoneManager.h"
#include "FlagSet.h"
#include "GCAddHelicopter.h"
#include "GCModifyInformation.h"
#include "GCNoticeEvent.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "Store.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "VariableManager.h"
#include "WayPoint.h"
#include "Zone.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "war/WarSystem.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectWayPointHandler::execute(CGSelectWayPoint* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__
    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    static map<Level_t, Price_t> sPriceMap;

    try {
        int targetDynamicZoneType =
            de::gameContext().dynamicZoneInfos().getDynamicZoneTypeByZoneID(pPacket->getZoneID());
        if (targetDynamicZoneType != DYNAMIC_ZONE_MAX) {
            executeEnterQuestZone(pPacket, pPlayer, targetDynamicZoneType);
        }

        // Just return if the game player's state is not normal.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);
        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        // Return if the creature is not a Slayer.
        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        if (pPC->getStore()->isOpen())
            return;
        if (pCreature->hasRelicItem())
            return;

        // Return if the creature is dead
        if (pCreature->isDead())
            return;

        // Entering the beginner zone is allowed regardless of race.
        if (pPacket->getZoneID() == 1122) {
            ZONE_COORD pos(1122);

            if (pCreature->isSlayer()) {
                pos.x = 107;
                pos.y = 27;
            } else if (pCreature->isVampire()) {
                pos.x = 18;
                pos.y = 27;
            } else if (pCreature->isOusters()) {
                pos.x = 12;
                pos.y = 103;
            } else
                return;

            if (!canEnterBeginnerZone(pCreature))
                return;

            pPC->getGQuestManager()->illegalWarp();
            transportCreature(pCreature, pos.id, pos.x, pos.y, false);
            return;
        }

        if (pPacket->getZoneID() == 1131) {
            if (de::gameContext().variables().getVariable(ACTIVE_LEVEL_WAR) == 0) {
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_ENTER));
                pGamePlayer->sendPacket(&gcSystemMessage);
                return;
            }

            // Look at the creature information and bounce it accordingly
            ZONE_COORD pos(de::gameContext().levelWarZones().getCreatureZoneID(pCreature));

            if (de::gameContext().sweeperBonuses().isAble(
                    de::gameContext().levelWarZones().getCreatureZoneID(pCreature))) {
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(strings.getString(STRID_NO_WAR_IN_ACTIVE));
                pGamePlayer->sendPacket(&gcSystemMessage);
                return;
            }

            if (pCreature->isSlayer()) {
                pos.x = 12;
                pos.y = 9;
            } else if (pCreature->isVampire()) {
                pos.x = 117;
                pos.y = 8;
            } else if (pCreature->isOusters()) {
                pos.x = 9;
                pos.y = 111;
            }

            pPC->getGQuestManager()->illegalWarp();
            transportCreature(pCreature, pos.id, pos.x, pos.y, false);
            return;
        }

        if (pPacket->getZoneID() == 72) {
            if (!de::gameContext().warSystem().hasActiveRaceWar()) {
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(strings.getString(STRID_NO_WAR_IN_ACTIVE));
                pGamePlayer->sendPacket(&gcSystemMessage);

                return;
            }

            // Look at the creature information and bounce it accordingly
            ZONE_COORD pos;

            if (pCreature->isSlayer()) {
                pos.id = 73;
                pos.x = 30;
                pos.y = 124;
            } else if (pCreature->isVampire()) {
                pos.id = 71;
                pos.x = 104;
                pos.y = 128;
            } else if (pCreature->isOusters()) {
                pos.id = 72;
                pos.x = 67;
                pos.y = 165;
            }
            if (!de::gameContext().variables().isActiveRaceWarLimiter() ||
                pCreature->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET)) {
                pPC->getGQuestManager()->illegalWarp();
                transportCreature(pCreature, pos.id, pos.x, pos.y, false);
                return;
            } else {
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_ENTER_DURING_RACE_WAR));
                pGamePlayer->sendPacket(&gcSystemMessage);
                return;
            }
        }

        if (!pCreature->isSlayer() && !pCreature->isOusters()) {
            // Should something be done here?
            return;
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_FLAG)) {
            // Should something be done here?
            return;
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
            // Should something be done here?
            return;
        }


        bool bCancel = false;

        // A normal move requires the effect to be attached.
        if (pCreature->isOusters() ||
            (pCreature->isSlayer() && pCreature->isFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL))) {
            ZoneID_t id = pPacket->getZoneID();
            ZoneCoord_t x = pPacket->getX();
            ZoneCoord_t y = pPacket->getY();

            if (id == 0 && x == 0 && y == 0) {
                bCancel = true;
            } else {
                // Ignored in the petrified state.
                if (pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE)) {
                    bCancel = true;
                }

                // Through the waypoint manager, verify that the waypoint the client sent
                // is a valid waypoint.
                if (!de::gameContext().wayPoints().isValidWayPoint(id, x, y, pCreature->getRace())) {
                    // Should something be done here?
                    bCancel = true;
                }

                try {
                    if (!bCancel) {
                        if (!bCancel) {
                            // Delete the effect before moving.
                            if (pCreature->isSlayer())
                                pCreature->removeFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL);

                            if (pCreature->isOusters()) {
                                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                                Assert(pOusters != NULL);

                                GCNoticeEvent gcNoticeEvent;

                                // Using the Gnome's Horn requires a contract with Sioram.
                                if (!pOusters->getFlagSet()->isOn(FLAGSET_GNOMES_HORN)) {
                                    gcNoticeEvent.setCode(NOTICE_EVENT_CONTRACT_GNOMES_HORN);
                                    pPlayer->sendPacket(&gcNoticeEvent);
                                    return;
                                }

                                Level_t level = pOusters->getLevel();
                                Price_t price = sPriceMap[level];

                                if (price == 0) {
                                    price = (Price_t)(pow((double)level, 1.3) * 100) / 2;
                                    sPriceMap[level] = price;
                                }


                                if (pOusters->getGold() < price) {
                                    gcNoticeEvent.setCode(NOTICE_EVENT_NOT_ENOUGH_MONEY);
                                    pPlayer->sendPacket(&gcNoticeEvent);
                                    return;
                                } else {
                                    pOusters->decreaseGoldEx(price);
                                    GCModifyInformation gcMI;
                                    gcMI.addLongData(MODIFY_GOLD, pOusters->getGold());
                                    pPlayer->sendPacket(&gcMI);
                                }
                            }

                            // On a valid waypoint, move the slayer.
                            pPC->getGQuestManager()->illegalWarp();
                            transportCreature(pCreature, id, x, y, false);
                        }
                    }
                } catch (NoSuchElementException&) {
                    bCancel = true;
                }
            }
        }

        if (bCancel && pCreature->isSlayer()) {
            Zone* pZone = pCreature->getZone();
            Assert(pZone != NULL);

            // id, x and y all 0 means the move is cancelled.
            pCreature->removeFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL);

            // Broadcast that the helicopter is to be removed.
            GCAddHelicopter gcAddHelicopter;
            gcAddHelicopter.setObjectID(pCreature->getObjectID());
            gcAddHelicopter.setCode(1);
            pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddHelicopter);
        }
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}


void CGSelectWayPointHandler::executeEnterQuestZone(CGSelectWayPoint* pPacket, Player* pPlayer,
                                                    int targetDynamicZoneType)

{
#ifdef __GAME_SERVER__

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    bool bQuestCondition = pPC->getGQuestManager()->canEnterDynamicZone(pPacket->getZoneID());

    DynamicZoneGroup* pDynamicZoneGroup = de::gameContext().dynamicZones().getDynamicZoneGroup(targetDynamicZoneType);
    Assert(pDynamicZoneGroup != NULL);

    bool bDynamicZoneAvailable = pDynamicZoneGroup->canEnter();

    if (bQuestCondition && bDynamicZoneAvailable) {
        DynamicZone* pDynamicZone = pDynamicZoneGroup->getAvailableDynamicZone();

        transportCreature(pPC, pDynamicZone->getZoneID(), 15, 15, true);

        pPC->getGQuestManager()->enterDynamicZone(pPacket->getZoneID());
    }

#endif
}
