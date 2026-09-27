//////////////////////////////////////////////////////////////////////////////
// Filename    : PCManager.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PCManager.h"

#include <stdio.h>

#include <algorithm> // find_if ()

#include "AlignmentManager.h"
#include "Assert.h"
#include "BloodBibleBonusManager.h"
#include "Creature.h"
#include "CreatureUtil.h"
#include "Event.h"
#include "EventResurrect.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "HolyLandManager.h"
#include "IncomingPlayerManager.h"
#include "Inventory.h"
#include "ItemFactoryManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "MonsterCorpse.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "SkillHandlerManager.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "Thread.h"
#include "Tile.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
// #include "SweeperBonusManager.h"
#include "EffectComa.h"
#include "EffectPKZoneResurrection.h"
#include "EffectSummonCasket.h"
#include "EffectTryingPosition.h"
#include "EventTransport.h"
#include "EventZoneInfo.h"
#include "GCAddEffect.h"
#include "GCCreatureDied.h"
#include "GCGetOffMotorCycle.h"
#include "GCHolyLandBonusInfo.h"
#include "GCRemoveEffect.h"
#include "GCRemoveFromGear.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "KernelContext.h"
#include "LevelWarZoneInfoManager.h"
#include "Properties.h"
#include "SiegeManager.h"
#include "skill/EffectHarpoonBomb.h"
#include "war/WarSystem.h"
// #include "GCSweeperBonusInfo.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
PCManager::PCManager()

{
    __BEGIN_TRY

    m_bRefreshHolyLandPlayer = false;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
// Save every object in the container to the database, then delete it.
//////////////////////////////////////////////////////////////////////////////
PCManager::~PCManager()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}


//////////////////////////////////////////////////////////////////////////////
// Run the heartbeat methods of the PCs belonging to the zone.
// A dead PC must not be removed here, because once a PC is removed from the PC
// manager its EffectManager's heartbeat method is no longer called.
// Removing it in CreatureDead::unaffect() instead would fit.
//
// This is where effect items are processed.
// This routine runs from the Zone heartbeat.
// With respect to other threads it runs after ProcessCommand has finished.
// It is not safe, however, if another thread calls PCManager::deleteCreature or
// addCreature.
//////////////////////////////////////////////////////////////////////////////
void PCManager::processCreatures()

{
    __BEGIN_TRY

    WarSystem& warSystem = de::gameContext().warSystem();

    __ENTER_CRITICAL_SECTION(m_Mutex)


    Timeval currentTime;
    getCurrentTime(currentTime);

    try {
        unordered_map<ObjectID_t, Creature*>::iterator before = m_Creatures.end();
        unordered_map<ObjectID_t, Creature*>::iterator current = m_Creatures.begin();

        while (current != m_Creatures.end()) {
            Creature* pCreature = current->second;
            Assert(pCreature != NULL);

            // Heartbeat of the items held; search inventory and gear.
            if (pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                Assert(pSlayer != NULL);
                pSlayer->heartbeat(currentTime);


                // Call initAllStat so the HolyLandRaceBonus is applied.
                if (m_bRefreshHolyLandPlayer && !warSystem.hasActiveRaceWar()) {
                    SLAYER_RECORD prev;

                    pSlayer->getSlayerRecord(prev);
                    pSlayer->initAllStat();
                    pSlayer->sendRealWearingInfo();
                    pSlayer->sendModifyInfo(prev);
                    // Resend the skill list for the holy land skills.
                    pSlayer->sendSlayerSkillInfo();
                }
            } else if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                Assert(pVampire != NULL);
                pVampire->heartbeat(currentTime);

                if (pVampire->isFlag(Effect::EFFECT_CLASS_COMA)) {
                    HP_t currentHP = pVampire->getHP(ATTR_CURRENT);
                    HP_t maxHP = pVampire->getHP(ATTR_MAX);

                    if (currentHP * 3 >= maxHP) {
                        EffectComa* pEffectComa =
                            dynamic_cast<EffectComa*>(pVampire->findEffect(Effect::EFFECT_CLASS_COMA));
                        Assert(pEffectComa != NULL);

                        if (pEffectComa->canResurrect()) {
                            // Delete the coma effect from the target's effect manager.
                            pVampire->deleteEffect(Effect::EFFECT_CLASS_COMA);
                            pVampire->removeFlag(Effect::EFFECT_CLASS_COMA);

                            // Tell the client the coma effect is gone.
                            GCRemoveEffect gcRemoveEffect;
                            gcRemoveEffect.setObjectID(pVampire->getObjectID());
                            gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_COMA);
                            pVampire->getZone()->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcRemoveEffect);

                            // Reattach the effects after the resurrection.
                            pVampire->getEffectManager()->sendEffectInfo(pVampire, pVampire->getZone(),
                                                                         pVampire->getX(), pVampire->getY());
                        }
                    }
                }

                // Call initAllStat so the HolyLandRaceBonus is applied.
                if (m_bRefreshHolyLandPlayer && !warSystem.hasActiveRaceWar()) {
                    VAMPIRE_RECORD prev;

                    pVampire->getVampireRecord(prev);
                    pVampire->initAllStat();
                    pVampire->sendRealWearingInfo();
                    pVampire->sendModifyInfo(prev);
                    // Resend the skill list for the holy land skills.
                    pVampire->sendVampireSkillInfo();
                }
            } else if (pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                Assert(pOusters != NULL);
                pOusters->heartbeat(currentTime);
            }

            if (pCreature->isDead()
                // Added for transfusion: isDead() checks whether HP is 0, but HP may
                // still be refilling.
                || (pCreature->isFlag(Effect::EFFECT_CLASS_COMA) && pCreature->isVampire())) {
                if (!pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                    ///////////////////////////////////////////////////////////////////
                    // Drop any relic item held on death to the ground.
                    ///////////////////////////////////////////////////////////////////
                    dropRelicToZone(pCreature);
                    dropFlagToZone(pCreature);
                    dropSweeperToZone(pCreature);

                    ///////////////////////////////////////////////////////////////////
                    // Drop items on death according to alignment.
                    ///////////////////////////////////////////////////////////////////
                    Zone* pZone = pCreature->getZone();
                    Assert(pZone != NULL);

                    Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());
                    EffectTryingPosition* pTryingTile =
                        dynamic_cast<EffectTryingPosition*>(rTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION));
                    if (pTryingTile != NULL) {
                        MonsterCorpse* pTower = pTryingTile->getTower();
                        Assert(pTower != NULL);

                        Effect* pTryingTower =
                            pTower->getEffectManager().findEffect(Effect::EFFECT_CLASS_SLAYER_TRYING_1);
                        if (pTryingTower != NULL)
                            pTryingTower->setDeadline(0);

                        pTryingTower = pCreature->findEffect(Effect::EFFECT_CLASS_TRYING);
                        if (pTryingTower != NULL)
                            pTryingTower->setDeadline(0);
                    }

                    Slayer* pSlayer = NULL;
                    Vampire* pVampire = NULL;
                    Ousters* pOusters = NULL;

                    Creature::CreatureClass CClass = pCreature->getCreatureClass();

                    if (CClass == Creature::CREATURE_CLASS_SLAYER) {
                        pSlayer = dynamic_cast<Slayer*>(pCreature);

                        int SumAttr =
                            pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) + pSlayer->getINT(ATTR_BASIC);

                        // A normal user with SumAttr over 40 may drop items on death.
                        if (SumAttr > 40 && pSlayer->getCompetence() == 3) {
                            Alignment_t alignment = pSlayer->getAlignment();
                            ItemNum_t DropItemNum =
                                de::gameContext().alignments().getDropItemNum(alignment, pSlayer->isPK());

                            // Drop the worn unique items, up to DropItemNum
                            // of them.
                            for (int i = 0; DropItemNum > 0 && i < Slayer::WEAR_MAX; i++) {
                                Item* pItem = pSlayer->getWearItem((Slayer::WearPart)i);

                                if (pItem != NULL && pItem->isUnique() && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pSlayer->removeShape(pItem->getItemClass(), true);


                                    if (isTwohandWeapon(pItem)) {
                                        pSlayer->deleteWearItem(Slayer::WEAR_LEFTHAND);
                                        pSlayer->deleteWearItem(Slayer::WEAR_RIGHTHAND);
                                    } else {
                                        pSlayer->deleteWearItem((Slayer::WearPart)i);
                                    }

                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pSlayer->getX(), pSlayer->getY());

                                    if (pt.x != -1) {
                                        filelog("uniqueItem.txt", "DropByKilled: %s %s", pSlayer->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }

                                    // Decrease the remaining drop count.
                                    DropItemNum--;

                                    // Only one unique item is ever dropped.
                                    break;
                                }
                            }


                            for (int i = 0; i < DropItemNum; i++) {
                                int RandomValue = Random(0, (int)Slayer::WEAR_MAX - 1);
                                Item* pItem = pSlayer->getWearItem(Slayer::WearPart(RandomValue));

                                // A couple ring must not be dropped.
                                // 2003.3.14
                                if (pItem != NULL && !isCoupleRing(pItem) && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pSlayer->removeShape(pItem->getItemClass(), true);

                                    if (isTwohandWeapon(pItem)) {
                                        pSlayer->deleteWearItem(Slayer::WEAR_LEFTHAND);
                                        pSlayer->deleteWearItem(Slayer::WEAR_RIGHTHAND);
                                    } else {
                                        pSlayer->deleteWearItem(Slayer::WearPart(RandomValue));
                                    }


                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pSlayer->getX(), pSlayer->getY());

                                    if (pt.x != -1) {
                                        filelog("dropItem.txt", "DropByKilled: %s %s", pSlayer->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }
                                }
                            }
                        }

                        // Reset the PK flag.
                        pSlayer->setPK(false);
                    } else if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
                        pVampire = dynamic_cast<Vampire*>(pCreature);

                        // A hidden vampire pops out on death.
                        // 2003. 1. 17. Sequoia, DEW
                        if (pVampire->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                            if (canUnburrow(pZone, pVampire->getX(), pVampire->getY())) {
                                addUnburrowCreature(pZone, pVampire, pVampire->getX(), pVampire->getY(),
                                                    pVampire->getDir());
                            }
                        }

                        // A vampire in bat or wolf form returns to its own form on death.
                        if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                            pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
                            pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
                            Zone* pZone = pVampire->getZone();
                            Assert(pZone != NULL);
                            addUntransformCreature(pZone, pVampire, true);
                        }

                        // A vampire inside a casket loses the casket on death.

                        // Drop money and items according to alignment.
                        if (pVampire->getLevel() > 10 && pVampire->getCompetence() == 3) {
                            Alignment_t alignment = pVampire->getAlignment();
                            ItemNum_t DropItemNum =
                                de::gameContext().alignments().getDropItemNum(alignment, pVampire->isPK());

                            // Drop the worn unique items, up to DropItemNum
                            // of them.
                            for (int i = 0; DropItemNum > 0 && i < Vampire::VAMPIRE_WEAR_MAX; i++) {
                                Item* pItem = pVampire->getWearItem((Vampire::WearPart)i);

                                if (pItem != NULL && pItem->isUnique() && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pVampire->removeShape(pItem->getItemClass(), true);

                                    if (isTwohandWeapon(pItem)) {
                                        pVampire->deleteWearItem(Vampire::WEAR_LEFTHAND);
                                        pVampire->deleteWearItem(Vampire::WEAR_RIGHTHAND);
                                    } else {
                                        pVampire->deleteWearItem((Vampire::WearPart)i);
                                    }

                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pVampire->getX(), pVampire->getY());

                                    if (pt.x != -1) {
                                        filelog("uniqueItem.txt", "DropByKilled: %s %s", pVampire->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }

                                    // Decrease the remaining drop count.
                                    DropItemNum--;

                                    // Only one unique item is ever dropped.
                                    break;
                                }
                            }


                            for (int i = 0; i < DropItemNum; i++) {
                                int RandomValue = Random(0, (int)Vampire::VAMPIRE_WEAR_MAX - 1);
                                Item* pItem = pVampire->getWearItem(Vampire::WearPart(RandomValue));

                                // A couple ring must not be dropped.
                                // 2003.3.14
                                if (pItem != NULL && !isCoupleRing(pItem) && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pVampire->removeShape(pItem->getItemClass(), true);

                                    if (isTwohandWeapon(pItem)) {
                                        pVampire->deleteWearItem(Vampire::WEAR_LEFTHAND);
                                        pVampire->deleteWearItem(Vampire::WEAR_RIGHTHAND);
                                    } else {
                                        pVampire->deleteWearItem(Vampire::WearPart(RandomValue));
                                    }

                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pVampire->getX(), pVampire->getY());

                                    if (pt.x != -1) {
                                        filelog("dropItem.txt", "DropByKilled: %s %s", pVampire->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }
                                }
                            }
                        }

                        SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SKILL_EXTREME);
                        Assert(pSkillHandler != NULL);
                        // Apply the Extreme effect.
                        pSkillHandler->execute(pVampire);

                        // Reset the PK flag.
                        pVampire->setPK(false);
                    } else if (CClass == Creature::CREATURE_CLASS_OUSTERS) {
                        pOusters = dynamic_cast<Ousters*>(pCreature);

                        // Drop money and items according to alignment.
                        if (pOusters->getLevel() > 10 && pOusters->getCompetence() == 3) {
                            Alignment_t alignment = pOusters->getAlignment();
                            ItemNum_t DropItemNum =
                                de::gameContext().alignments().getDropItemNum(alignment, pOusters->isPK());

                            // Drop the worn unique items, up to DropItemNum
                            // of them.
                            for (int i = 0; DropItemNum > 0 && i < Ousters::OUSTERS_WEAR_MAX; i++) {
                                Item* pItem = pOusters->getWearItem((Ousters::WearPart)i);

                                if (pItem != NULL && pItem->isUnique() && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pOusters->removeShape(pItem->getItemClass(), true);

                                    if (isTwohandWeapon(pItem)) {
                                        pOusters->deleteWearItem(Ousters::WEAR_LEFTHAND);
                                        pOusters->deleteWearItem(Ousters::WEAR_RIGHTHAND);
                                    } else {
                                        pOusters->deleteWearItem((Ousters::WearPart)i);
                                    }

                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pOusters->getX(), pOusters->getY());

                                    if (pt.x != -1) {
                                        filelog("uniqueItem.txt", "DropByKilled: %s %s", pOusters->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }

                                    // Decrease the remaining drop count.
                                    DropItemNum--;

                                    // Only one unique item is ever dropped.
                                    break;
                                }
                            }

                            for (int i = 0; i < DropItemNum; i++) {
                                int RandomValue = Random(0, (int)Ousters::OUSTERS_WEAR_MAX - 1);
                                Item* pItem = pOusters->getWearItem(Ousters::WearPart(RandomValue));

                                // A couple ring must not be dropped.
                                // 2003.3.14
                                if (pItem != NULL && !isCoupleRing(pItem) && !pItem->isTimeLimitItem()) {
                                    // by sigi. 2002.11.7
                                    pOusters->removeShape(pItem->getItemClass(), true);

                                    if (isTwohandWeapon(pItem)) {
                                        pOusters->deleteWearItem(Ousters::WEAR_LEFTHAND);
                                        pOusters->deleteWearItem(Ousters::WEAR_RIGHTHAND);
                                    } else {
                                        pOusters->deleteWearItem(Ousters::WearPart(RandomValue));
                                    }

                                    // Scatter it into the zone.
                                    TPOINT pt = pZone->addItem(pItem, pOusters->getX(), pOusters->getY());

                                    if (pt.x != -1) {
                                        filelog("dropItem.txt", "DropByKilled: %s %s", pOusters->getName().c_str(),
                                                pItem->toString().c_str());
                                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            char zoneName[15];
                                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x,
                                                     pt.y);
                                            remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_MOVE,
                                                              pZone->getZoneID(), pt.x, pt.y);
                                        }
                                    } else {
                                        // Write an ItemTraceLog entry.
                                        if (pItem != NULL && pItem->isTraceItem()) {
                                            remainTraceLog(pItem, pCreature->getName(), "GOD", ITEM_LOG_DELETE,
                                                           DETAIL_DROP);
                                            remainTraceLogNew(pItem, pCreature->getName(), ITL_DROP, ITLD_DELETE);
                                        }
                                        pItem->destroy();
                                        SAFE_DELETE(pItem);
                                    }
                                }
                            }
                        }

                        // Reset the PK flag.
                        pOusters->setPK(false);
                    } else {
                        throw Error("invalid creature class");
                    }

                    // On the first death COMA is not set, so this branch is taken
                    // and COMA is applied.
                    EffectComa* pEffectComa = new EffectComa(pCreature);
                    pEffectComa->setStartTime();
                    if (pTryingTile != NULL)
                        pEffectComa->setDeadline(0);
                    else
                        pEffectComa->setDeadline(600);

                    EffectManager* pEffectManager = pCreature->getEffectManager();
                    Assert(pEffectManager != NULL);
                    pEffectManager->addEffect(pEffectComa);

                    pCreature->setFlag(Effect::EFFECT_CLASS_COMA);

                    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pCreature);
                    Assert(pPlayerCreature != NULL);

                    pPlayerCreature->getGQuestManager()->killed();


                    if (pCreature->isSlayer()) {
                        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                        // Dismount the motorcycle if riding one.
                        if (pSlayer->hasRideMotorcycle()) {
                            Zone* pZone = pCreature->getZone();
                            Assert(pZone != NULL);

                            pSlayer->getOffMotorcycle();
                            GCGetOffMotorCycle _GCGetOffMotorCycle;
                            _GCGetOffMotorCycle.setObjectID(pSlayer->getObjectID());
                            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCGetOffMotorCycle);
                        }

                        // Turn off the helicopter if one has been called.
                        if (pSlayer->isFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL)) {
                            pSlayer->removeFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL);

                            // A packet removing the helicopter should be broadcast here.
                        }
                    } else if (pCreature->isVampire()) {
                        //  Leave the casket.
                        if (pCreature->isFlag(Effect::EFFECT_CLASS_CASKET)) {
                            Effect* pEffectCasket = pCreature->findEffect(Effect::EFFECT_CLASS_CASKET);

                            if (pEffectCasket != NULL) {
                                pEffectCasket->unaffect();
                            }

                            pCreature->deleteEffect(Effect::EFFECT_CLASS_CASKET);
                            pCreature->removeFlag(Effect::EFFECT_CLASS_CASKET);
                        }
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_EXPLOSION_WATER);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_EXPLOSION_WATER);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_INSTALL_TURRET);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_INSTALL_TURRET);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_INSTALL_TURRET);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_DIVINE_GUIDANCE)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_DIVINE_GUIDANCE);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_DIVINE_GUIDANCE);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_DIVINE_GUIDANCE);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_GLACIER)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_GLACIER);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_GLACIER);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_GLACIER);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_ACID_ERUPTION)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_ACID_ERUPTION);
                        if (pEffect != NULL)
                            pEffect->unaffect();
                        pCreature->removeFlag(Effect::EFFECT_CLASS_ACID_ERUPTION);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_ACID_ERUPTION);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT)) {
                        pCreature->removeFlag(Effect::EFFECT_CLASS_FADE_OUT);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_FADE_OUT);
                        // unaffect must not be called: it would set sniping or invisibility.
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET)) {
                        pCreature->removeFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET);
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_REFINIUM_TICKET);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_SUMMON_SYLPH);

                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }
                        // Remove the sylph summon effect.
                        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_SUMMON_SYLPH);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_INVISIBILITY);

                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }
                        // Remove the invisibility effect.
                        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_INVISIBILITY);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_INVISIBILITY);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_SNIPING_MODE);

                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }
                        // Remove the sniping-mode effect.
                        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_SNIPING_MODE);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_SNIPING_MODE);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE)) {
                        Effect* pEffectPal = pCreature->findEffect(Effect::EFFECT_CLASS_PARALYZE);

                        if (pEffectPal != NULL) {
                            pEffectPal->unaffect();
                        }
                        // Remove paralyze.
                        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_PARALYZE);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_PARALYZE);
                    }

                    // Turn off hallucination on death.
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_HALLUCINATION)) {
                        Effect* pEffectHallu = pCreature->findEffect(Effect::EFFECT_CLASS_HALLUCINATION);

                        if (pEffectHallu != NULL) {
                            pEffectHallu->unaffect();
                        }

                        pCreature->deleteEffect(Effect::EFFECT_CLASS_HALLUCINATION);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_HALLUCINATION);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_MAGNUM_SPEAR)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_MAGNUM_SPEAR);
                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_MAGNUM_SPEAR);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_MAGNUM_SPEAR);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);
                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);
                    }

                    // Turn off the Soul Chain effect on death.
                    // If the flag is off when unaffect runs, no transport happens.
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN)) {
                        pCreature->removeFlag(Effect::EFFECT_CLASS_SOUL_CHAIN);
                    }

                    // Turn off the Love Chain effect on death.
                    // If the flag is off when unaffect runs, no transport happens.
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN)) {
                        pCreature->removeFlag(Effect::EFFECT_CLASS_LOVE_CHAIN);
                    }

                    // Turn off the GunShotGuidance Aim effect on death.
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM)) {
                        pCreature->deleteEffect(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM);
                    }

                    // Turn off the Sleep / Armageddon effect on death.
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP)) {
                        Effect* pEffectSleep = pCreature->findEffect(Effect::EFFECT_CLASS_SLEEP);

                        if (pEffectSleep != NULL) {
                            pEffectSleep->unaffect();
                        }

                        pCreature->deleteEffect(Effect::EFFECT_CLASS_SLEEP);
                    }
                    if (pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON)) {
                        Effect* pEffectArma = pCreature->findEffect(Effect::EFFECT_CLASS_ARMAGEDDON);

                        if (pEffectArma != NULL) {
                            pEffectArma->unaffect();
                        }

                        pCreature->deleteEffect(Effect::EFFECT_CLASS_ARMAGEDDON);
                    }

                    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED)) {
                        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_TRAPPED);

                        if (pEffect != NULL) {
                            pEffect->unaffect();
                        }

                        pCreature->deleteEffect(Effect::EFFECT_CLASS_TRAPPED);
                    }

                    // Broadcast the effect that lays the creature on the ground.
                    GCAddEffect gcAddEffect;
                    gcAddEffect.setObjectID(pCreature->getObjectID());
                    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_COMA);
                    gcAddEffect.setDuration(300);
                    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);

                    if (de::kernelContext().config().hasKey("Hardcore") &&
                        de::kernelContext().config().getPropertyInt("Hardcore") != 0) {
                        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                        Assert(pPC != NULL);

                        deletePC(pPC);
                        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
                        Assert(pGamePlayer != NULL);

                        filelog("DeletePC.log", "Deleted after death : %s", pPC->getName().c_str());

                        pGamePlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                        pGamePlayer->setItemRatioBonusPoint(1);
                    }

                    before = current++;
                } else {
                    EffectManager* pEffectManager = pCreature->getEffectManager();
                    Assert(pEffectManager != NULL);

                    EffectComa* pEffectComa = (EffectComa*)(pEffectManager->findEffect(Effect::EFFECT_CLASS_COMA));
                    Assert(pEffectComa != NULL);

                    if (pEffectComa->getDeadline() < currentTime) {
                        // Delete the effect first.
                        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_COMA);
                        pCreature->removeFlag(Effect::EFFECT_CLASS_COMA);

                        // The coma deadline has passed, so the PC is really dead: kill it and
                        // move the player from ZPM to IPM.
                        killCreature(pCreature);

                        // Delete the PC's node.
                        m_Creatures.erase(current);

                        if (before == m_Creatures.end()) // first element
                        {
                            current = m_Creatures.begin();
                        } else // !first element
                        {
                            current = before;
                            current++;
                        }
                    } else {
                        before = current++;
                    }
                }
            } else {
                before = current++;

                // Run the effects attached to the creature.
                pCreature->getEffectManager()->heartbeat(currentTime);
            }
        }

        // Not sent during a war.
        if (m_bRefreshHolyLandPlayer && !warSystem.hasActiveRaceWar()) {
            // Broadcast the blood bible bonus information across Adam's holy land.
            GCHolyLandBonusInfo gcHolyLandBonusInfo;
            de::gameContext().bloodBibleBonuses().makeHolyLandBonusInfo(gcHolyLandBonusInfo);
            de::gameContext().holyLands().broadcast(&gcHolyLandBonusInfo);
        }


        m_bRefreshHolyLandPlayer = false;

    } catch (Throwable& t) {
        filelog("PCManagerBug.log", "ProcessCreatureBug : %s", t.toString().c_str());
    }


    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// (1) Remove the creature from its tile.
// (2) Create the corpse and move the inventory into it.
// (3) Handle any item already on the tile.
// (4) Add the corpse to the tile, or next to it if the tile is taken.
// (5) Add the resurrection effect to the creature.
//////////////////////////////////////////////////////////////////////////////
void PCManager::killCreature(Creature* pDeadCreature)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(pDeadCreature != NULL);

    PKZoneInfoManager& pkZoneInfos = de::gameContext().pkZoneInfos();

    // Because of transfusion, HP keeps filling even after death.
    // Ignored.

    Zone* pZone = pDeadCreature->getZone();
    Assert(pZone != NULL);

    ZoneCoord_t cx = pDeadCreature->getX();
    ZoneCoord_t cy = pDeadCreature->getY();

    // Eternity
    pDeadCreature->removeFlag(Effect::EFFECT_CLASS_ETERNITY);

    // Dying in a PK zone attaches an effect on resurrection.
    if (pkZoneInfos.isPKZone(pZone->getZoneID())) {
        EffectPKZoneResurrection* pEffect = new EffectPKZoneResurrection(pDeadCreature);
        pDeadCreature->addEffect(pEffect);
        pDeadCreature->setFlag(pEffect->getEffectClass());
    }

    // Delete the PartyInviteInfo if a party invitation is pending.
    PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
    pPIIM->cancelInvite(pDeadCreature);

    // Remove the dead creature from the local party manager first.
    uint PartyID = pDeadCreature->getPartyID();
    if (PartyID != 0) {
        LocalPartyManager* pLPM = pZone->getLocalPartyManager();
        pLPM->deletePartyMember(PartyID, pDeadCreature);
    }

    // Delete the trade information if a trade was in progress.
    TradeManager* pTradeManager = pZone->getTradeManager();
    TradeInfo* pInfo = pTradeManager->getTradeInfo(pDeadCreature->getName());
    if (pInfo != NULL) {
        pTradeManager->cancelTrade(pDeadCreature);
    }

    // Remove the EFFECT_CLASS_CANNOT_ABSORB_SOUL effect from the corpse.
    if (pDeadCreature->isFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL)) {
        pDeadCreature->removeFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL);
    }

    bool addCorpse = true;
    if (pDeadCreature->isFlag(Effect::EFFECT_CLASS_HARPOON_BOMB)) {
        Effect* pEffect = pDeadCreature->findEffect(Effect::EFFECT_CLASS_HARPOON_BOMB);
        if (pEffect == NULL)
            addCorpse = false;
    }

    if (addCorpse) {
        // Create the corpse.
        Corpse* pCorpse = NULL;

        if (pDeadCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pDeadCreature);
            pCorpse = new SlayerCorpse(pSlayer);
            pCorpse->setLevel((int)(pSlayer->getHighestSkillDomainLevel()));
            pCorpse->setExp((Exp_t)computeCreatureExp(pSlayer, BLOODDRAIN_EXP));
        } else if (pDeadCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pDeadCreature);
            pCorpse = new VampireCorpse(pVampire);
            pCorpse->setLevel((int)(pVampire->getLevel()));
            pCorpse->setExp((Exp_t)computeCreatureExp(pVampire, BLOODDRAIN_EXP));
        } else if (pDeadCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pDeadCreature);
            pCorpse = new OustersCorpse(pOusters);
            pCorpse->setLevel((int)(pOusters->getLevel()));
            pCorpse->setExp((Exp_t)computeCreatureExp(pOusters, BLOODDRAIN_EXP));
        } else {
            throw Error("PlayerCreature class type error");
        }

        // by sigi. 2002.12.12
        addCorpseToZone(pCorpse, pZone, cx, cy);

        if (pDeadCreature->isFlag(Effect::EFFECT_CLASS_HARPOON_BOMB)) {
            EffectHarpoonBomb* pEffect =
                dynamic_cast<EffectHarpoonBomb*>(pDeadCreature->findEffect(Effect::EFFECT_CLASS_HARPOON_BOMB));
            if (pEffect != NULL) {
                EffectHarpoonBomb* pZoneEffect = new EffectHarpoonBomb(pZone, pCorpse->getX(), pCorpse->getY());
                pZoneEffect->setDamage(pEffect->getDamage());
                pZoneEffect->setUserObjectID(pEffect->getUserObjectID());
                pZoneEffect->setNextTime(pEffect->getNextTime());
                pZoneEffect->setDeadline(pEffect->getRemainDuration());
                pZone->registerObject(pZoneEffect);
                pZone->getTile(pCorpse->getX(), pCorpse->getY()).addEffect(pZoneEffect);
                pZone->addEffect(pZoneEffect);
            }
        }
    }

    // Tell the surroundings that the creature died.
    GCCreatureDied gcCreatureDied;
    gcCreatureDied.setObjectID(pDeadCreature->getObjectID());
    pDeadCreature->getPlayer()->sendPacket(&gcCreatureDied);
    pZone->broadcastPacket(cx, cy, &gcCreatureDied, pDeadCreature);

    // Take the creature off the tile only. It must stay in the PCManager: dropping
    // it from there would stop its effect manager's heartbeat.
    Assert(pZone->getTile(cx, cy).getCreature(pDeadCreature->getMoveMode()) == pDeadCreature);
    pZone->deleteCreatureFromTile(pDeadCreature, cx, cy);


    // *NOTE
    // This sets the destination in advance in case the connection is forcibly closed.
    // It is tied closely to the Resurrect handling, so Resurrect must be considered
    // if it changes. A plain database save is not enough: GamePlayer saves the
    // creature while disconnecting and would overwrite it.
    ZoneID_t ZoneID = 0;
    ZoneCoord_t ZoneX = 0;
    ZoneCoord_t ZoneY = 0;
    ZONE_COORD ResurrectCoord;
    Zone* pResurrectZone = NULL;

    // Find which server and zone group the destination zone belongs to.
    // Every zone should name the zone to return to when a creature dies in it.
    ZoneInfo* pZoneInfo = NULL;
    ZoneGroup* pZoneGroup = NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pDeadCreature);
    Assert(pPC != NULL);

    // Dying in an event zone resurrects at the event zone's resurrection position.
    EventZoneInfo* pEventZoneInfo = EventZoneInfoManager::Instance().getEventZoneInfo(pPC->getZoneID());
    if (pEventZoneInfo != NULL) {
        ResurrectCoord.id = pPC->getZoneID();
        ResurrectCoord.x = pEventZoneInfo->getResurrectX();
        ResurrectCoord.y = pEventZoneInfo->getResurrectY();
    }
    // Dying in a PK zone resurrects at the PK zone's
    // resurrection position.
    else if (pkZoneInfos.isPKZone(pPC->getZoneID())) {
        if (!pkZoneInfos.getResurrectPosition(pPC->getZoneID(), ResurrectCoord))
            de::gameContext().resurrectLocations().getPosition(pPC, ResurrectCoord);
    }
    // Illusion Way 1.
    else if (pPC->getZoneID() == 1410) {
        ResurrectCoord.id = 1410;
        ResurrectCoord.x = 120;
        ResurrectCoord.y = 70;
    } else if (pPC->getZoneID() == 1411) {
        ResurrectCoord.id = 1411;
        ResurrectCoord.x = 126;
        ResurrectCoord.y = 60;
    } else if (SiegeManager::Instance().isSiegeZone(pPC->getZoneID())) {
        ResurrectCoord.id = pPC->getZoneID();
        if (pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_DEFENDER) || pPC->isFlag(Effect::EFFECT_CLASS_SIEGE_REINFORCE)) {
            ResurrectCoord.x = 172;
            ResurrectCoord.y = 38;
        } else {
            ResurrectCoord.x = 20;
            ResurrectCoord.y = 232;
        }
    } else {
        de::gameContext().resurrectLocations().getPosition(pPC, ResurrectCoord);
    }

    ZoneID = ResurrectCoord.id;
    ZoneX = ResurrectCoord.x;
    ZoneY = ResurrectCoord.y;

    pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneID);
    pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(pZoneInfo->getZoneGroupID());

    // Associate the Resurrect event with the player object.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pDeadCreature->getPlayer());
    EventResurrect* pEventResurrect = new EventResurrect(pGamePlayer);
    pEventResurrect->setDeadline(0);

    // Set the resurrection location.
    pResurrectZone = pZoneGroup->getZone(ZoneID);
    Assert(pResurrectZone != NULL);

    if (pZone->isHolyLand() != pResurrectZone->isHolyLand()) {
        // Resurrecting across the boundary of Adam's holy land requires initAllStat
        // so the holy land bonus is set again.
        pDeadCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
    }

    if (de::gameContext().levelWarZones().isCreatureBonusZone(pDeadCreature, pZone->getZoneID()) !=
        de::gameContext().levelWarZones().isCreatureBonusZone(pDeadCreature, pResurrectZone->getZoneID())) {
        pDeadCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
    }

    if (pZone->isLevelWarZone() != pResurrectZone->isLevelWarZone()) {
        pDeadCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
    }

    pDeadCreature->setNewZone(pResurrectZone);
    pDeadCreature->setNewXY(ZoneX, ZoneY);

    // Attach the event to the player.
    pGamePlayer->addEvent(pEventResurrect);

    // For the database save, EventResurrect changes the zone setting.
    ZoneCoord_t oldZoneX = pDeadCreature->getX();
    ZoneCoord_t oldZoneY = pDeadCreature->getY();

    // Save the new zone information.
    pDeadCreature->setZone(pResurrectZone);
    pDeadCreature->setXY(ZoneX, ZoneY);

    pDeadCreature->save();
    // Restore the original zone information.
    pDeadCreature->setZone(pZone);
    pDeadCreature->setXY(oldZoneX, oldZoneY);

    pGamePlayer->setPlayerStatus(GPS_IGNORE_ALL);

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// transport All Creatures
//////////////////////////////////////////////////////////////////////////////
void PCManager::transportAllCreatures(ZoneID_t ZoneID, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY, Race_t race,
                                      Turn_t delay) const

{
    __BEGIN_TRY

    vector<ObjectID_t> creatureIDs;

    Zone* pZone = NULL;

    // transportCreature reaches back into PCManager from Zone::deleteCreature and
    // causes a SELF_DEAD_LOCK, so the IDs of every creature in the zone are
    // collected first.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<ObjectID_t, Creature*>::const_iterator iCreature = m_Creatures.begin();
    while (iCreature != m_Creatures.end()) {
        Creature* pCreature = iCreature->second;
        Assert(pCreature->isPC());

        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

        // race == 0xFF is the default and means every race;
        // otherwise only that race is transported.
        if (race == defaultRaceValue || race == pPC->getRace()) {
            creatureIDs.push_back(iCreature->first);

            pZone = pCreature->getZone();
        }

        iCreature++;
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // Move those creatures elsewhere.
    try {
        vector<ObjectID_t>::const_iterator itr = creatureIDs.begin();

        while (itr != creatureIDs.end()) {
            Creature* pCreature = pZone->getCreature(*itr);

            if (pCreature != NULL) {
                GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
                EventTransport* pEventTransport =
                    dynamic_cast<EventTransport*>(pGamePlayer->getEvent(Event::EVENT_CLASS_TRANSPORT));
                bool newEvent = false;
                if (pEventTransport == NULL) {
                    pEventTransport = new EventTransport(pGamePlayer);
                    newEvent = true;
                }

                if (ZoneID == 0xffff) {
                    ZONE_COORD ResurrectCoord;
                    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                    Assert(pPC != NULL);
                    de::gameContext().resurrectLocations().getPosition(pPC, ResurrectCoord);

                    // 10 seconds
                    pEventTransport->setDeadline(100);
                    pEventTransport->setZoneName("");
                    pEventTransport->setTargetZone(ResurrectCoord.id, ResurrectCoord.x, ResurrectCoord.y);
                } else {
                    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneID);
                    Assert(pZoneInfo != NULL);

                    pEventTransport->setDeadline(delay * 10);
                    pEventTransport->setZoneName(pZoneInfo->getFullName());

                    if (ZoneX == 0xffff) {
                        pEventTransport->setTargetZone(ZoneID, pCreature->getX(), pCreature->getY());
                    } else {
                        pEventTransport->setTargetZone(ZoneID, ZoneX, ZoneY);
                        // Tell the client where it moves to and after how many seconds.
                    }
                }

                if (newEvent)
                    pGamePlayer->addEvent(pEventTransport);
            }

            itr++;
        }
    } catch (Throwable& t) {
        filelog("PCManagerBug.log", "transportAllCreaturesBUG : %s", t.toString().c_str());
    }


    __END_CATCH
}

vector<uint> PCManager::getPCNumByRace() const {
    vector<uint> ret;
    ret.push_back(0);
    ret.push_back(0);
    ret.push_back(0);

    unordered_map<ObjectID_t, Creature*>::const_iterator itr = getCreatures().begin();

    for (; itr != getCreatures().end(); ++itr) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(itr->second);
        if (pPC != NULL) {
            ret[pPC->getRace()]++;
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string PCManager::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "PCManager(" << CreatureManager::toString() << ")";

    return msg.toString();

    __END_CATCH
}
