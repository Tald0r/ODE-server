//////////////////////////////////////////////////////////////////////////////
// Filename    : Restore.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Restore.h"

#include <stdio.h>

#include "DB.h"
#include "EffectBloodDrain.h"
#include "EffectRestore.h"
#include "GCDeleteObject.h"
#include "GCMorph1.h"
#include "GCMorphSlayer2.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToSelfOK1.h"
#include "GSGuildMemberLogOn.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "ItemUtil.h"
#include "NPC.h"
#include "PCFinder.h"
#include "Party.h"
#include "RelicUtil.h"
#include "SharedServerManager.h"
#include "TradeManager.h"
#include "repository/EffectSaveRepository.h"
#include "repository/SessionRepository.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void Restore::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pFromCreature = pZone->getCreature(TargetObjectID);

        // Only a Vampire can be targeted.
        // A missing target fails the skill instead of throwing.
        if (pFromCreature == NULL || !pFromCreature->isVampire()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1; // To the one who used the skill.
        GCMorph1 _GCMorph1;                     // To the one being transformed.
        GCMorphSlayer2 _GCMorphSlayer2;         // To the onlookers of the transformation.

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        bool bRangeCheck = verifyDistance(pSlayer, pFromCreature, pSkillInfo->getRange());
        bool bHitRoll = true;

        if (bRangeCheck && bHitRoll) {
            dropRelicToZone(pFromCreature);
            dropFlagToZone(pFromCreature);

            //////////////////////////////////////////////////////////////////////
            // Zone level information of every kind has to be deleted.
            //////////////////////////////////////////////////////////////////////

            // Clear the invite information if a party invite is pending.
            PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
            Assert(pPIIM != NULL);
            pPIIM->cancelInvite(pFromCreature);

            // Delete the party related information.
            int PartyID = pFromCreature->getPartyID();
            if (PartyID != 0) {
                // Delete it from the local manager first.
                LocalPartyManager* pLPM = pZone->getLocalPartyManager();
                Assert(pLPM != NULL);
                pLPM->deletePartyMember(PartyID, pFromCreature);

                // Delete it from the global manager too.
                deleteAllPartyInfo(pFromCreature);
            }

            // Cancel the trade information if a trade was in progress.
            TradeManager* pTM = pZone->getTradeManager();
            Assert(pTM != NULL);
            pTM->cancelTrade(pFromCreature);

            //////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////

            Slayer* pNewSlayer = new Slayer;
            Vampire* pVampire = dynamic_cast<Vampire*>(pFromCreature);

            // Delete any blood drain information still left in the database.
            defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, pFromCreature->getName());

            pNewSlayer->setName(pFromCreature->getName());

            // Update the player pointer in the creature and the creature pointer in the player.
            Player* pFromPlayer = pFromCreature->getPlayer();
            pNewSlayer->setPlayer(pFromPlayer);
            GamePlayer* pFromGamePlayer = dynamic_cast<GamePlayer*>(pFromPlayer);
            pFromGamePlayer->setCreature(pNewSlayer);

            // load() takes an object id from the zone it loads in, so set it again.
            pNewSlayer->load();
            pNewSlayer->setZone(pZone);
            pNewSlayer->setObjectID(pFromCreature->getObjectID());
            pNewSlayer->setMoveMode(Creature::MOVE_MODE_WALKING);

            ZoneCoord_t x = pFromCreature->getX();
            ZoneCoord_t y = pFromCreature->getY();
            Dir_t dir = pFromCreature->getDir();

            // pFromCreature, the original Vampire object, is about to be deleted,
            // so the value held in PCFinder would become garbage.
            // Therefore remove the Vampire pointer and add the new Slayer pointer.
            de::gameContext().playerCreatures().deleteCreature(pFromCreature->getName());
            de::gameContext().playerCreatures().addCreature(pNewSlayer);

            // Remove it from the guild's list of currently connected members.
            if (pVampire->getGuildID() != 0) {
                Guild* pGuild = de::gameContext().guilds().getGuild(pVampire->getGuildID());
                if (pGuild != NULL) {
                    pGuild->deleteCurrentMember(pVampire->getName());

                    GSGuildMemberLogOn gsGuildMemberLogOn;
                    gsGuildMemberLogOn.setGuildID(pGuild->getID());
                    gsGuildMemberLogOn.setName(pVampire->getName());
                    gsGuildMemberLogOn.setLogOn(false);

                    de::gameContext().sharedServer().sendPacket(&gsGuildMemberLogOn);

                    // Update the database.
                    defaultSessionRepository().markGuildMemberLoggedOff(pVampire->getName());
                } else
                    filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pVampire->getGuildID(),
                            pVampire->getName().c_str());
            }

            // Swap the inventory.
            Inventory* pInventory = pVampire->getInventory();
            pNewSlayer->setInventory(pInventory);
            pVampire->setInventory(NULL);

            // Swap the stash
            pNewSlayer->deleteStash();
            pNewSlayer->setStash(pVampire->getStash());
            pNewSlayer->setStashNum(pVampire->getStashNum());
            pNewSlayer->setStashStatus(false);
            pVampire->setStash(NULL);


            // Swap the flag set
            pNewSlayer->deleteFlagSet();
            pNewSlayer->setFlagSet(pVampire->getFlagSet());
            pVampire->setFlagSet(NULL);

            Item* pItem = NULL;
            _TPOINT point;

            // Move the worn items into the inventory or onto the ground.
            for (int part = 0; part < (int)Vampire::VAMPIRE_WEAR_MAX; part++) {
                pItem = pVampire->getWearItem((Vampire::WearPart)part);
                if (pItem != NULL) {
                    // Remove it from the gear first.
                    if (isTwohandWeapon(pItem)) {
                        Assert(((Vampire::WearPart)part == Vampire::WEAR_RIGHTHAND) ||
                               ((Vampire::WearPart)part == Vampire::WEAR_LEFTHAND));
                        Assert(pVampire->getWearItem(Vampire::WEAR_RIGHTHAND) ==
                               pVampire->getWearItem(Vampire::WEAR_LEFTHAND));
                        // Two-handed item.
                        pVampire->deleteWearItem(Vampire::WEAR_RIGHTHAND);
                        pVampire->deleteWearItem(Vampire::WEAR_LEFTHAND);
                    } else {
                        pVampire->deleteWearItem((Vampire::WearPart)part);
                    }

                    // If the inventory has room, add it there.
                    if (pInventory->getEmptySlot(pItem, point)) {
                        pInventory->addItem(point.x, point.y, pItem);
                        pItem->save(pNewSlayer->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
                    } else if (pItem->isTimeLimitItem()) {
                        pVampire->deleteItemByMorph(pItem);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    }
                    // Drop it on the ground when there is no room.
                    else {
                        ZoneCoord_t ZoneX = pVampire->getX();
                        ZoneCoord_t ZoneY = pVampire->getY();

                        TPOINT pt;

                        pt = pZone->addItem(pItem, ZoneX, ZoneY);

                        if (pt.x != -1) {
                            pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                            // Write an ItemTraceLog entry
                            if (pItem != NULL && pItem->isTraceItem()) {
                                char zoneName[15];
                                snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                                remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                            }
                        } else {
                            // Write an ItemTraceLog entry
                            if (pItem != NULL && pItem->isTraceItem()) {
                                remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                            }

                            pItem->destroy();
                            SAFE_DELETE(pItem);
                        }
                    }
                }
            }

            pItem = pVampire->getExtraInventorySlotItem();
            if (pItem != NULL) {
                pVampire->deleteItemFromExtraInventorySlot();

                // If the inventory has room, add it there.
                if (pInventory->getEmptySlot(pItem, point)) {
                    pInventory->addItem(point.x, point.y, pItem);
                    pItem->save(pNewSlayer->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
                } else if (pItem->isTimeLimitItem()) {
                    pVampire->deleteItemByMorph(pItem);

                    pItem->destroy();
                    SAFE_DELETE(pItem);
                }
                // Drop it on the ground when there is no room.
                else {
                    TPOINT pt;
                    ZoneCoord_t ZoneX = pVampire->getX();
                    ZoneCoord_t ZoneY = pVampire->getY();

                    pt = pZone->addItem(pItem, ZoneX, ZoneY);

                    if (pt.x != -1) {
                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                        // Write an ItemTraceLog entry
                        if (pItem != NULL && pItem->isTraceItem()) {
                            char zoneName[15];
                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                            remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                        }
                    } else {
                        // Write an ItemTraceLog entry
                        if (pItem != NULL && pItem->isTraceItem()) {
                            remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                        }

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    }
                }
            }

            // Reload the quest item information.
            pNewSlayer->loadTimeLimitItem();

            // The money the Vampire held does not carry over to the Slayer:
            // the new Slayer starts with none, in hand or in the stash.
            pNewSlayer->setGoldEx(0);
            pNewSlayer->setStashGoldEx(0);

            // Send the skill information.
            pNewSlayer->sendSlayerSkillInfo();


            // Delete the old vampire from the tile and the zone and add the new slayer,
            // on the nearest tile that will take it.
            pZone->replacePC(pFromCreature, pNewSlayer, x, y, dir, true);

            // Update the field of view.
            pZone->updateHiddenScan(pNewSlayer);

            _GCMorph1.setPCInfo2(pNewSlayer->getSlayerInfo2());
            _GCMorph1.setInventoryInfo(pNewSlayer->getInventoryInfo());
            _GCMorph1.setGearInfo(pNewSlayer->getGearInfo());
            _GCMorph1.setExtraInfo(pNewSlayer->getExtraInfo());

            _GCMorphSlayer2.setSlayerInfo(pNewSlayer->getSlayerInfo3());

            pFromPlayer->sendPacket(&_GCMorph1);


            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);


            GCDeleteObject _GCDeleteObject;
            _GCDeleteObject.setObjectID(TargetObjectID);
            pZone->broadcastPacket(x, y, &_GCDeleteObject, pNewSlayer);
            pZone->broadcastPacket(x, y, &_GCMorphSlayer2, pNewSlayer);


            pNewSlayer->tinysave("Race='SLAYER'");
            SAFE_DELETE(pFromCreature);


            pSkillSlot->setRunTime(0);

            EffectRestore* pEffectRestore = new EffectRestore(pNewSlayer);
            pEffectRestore->setDeadline(60 * 60 * 24 * 7 * 10); // 7 days
            pNewSlayer->addEffect(pEffectRestore);
            pNewSlayer->setFlag(Effect::EFFECT_CLASS_RESTORE);
            pEffectRestore->create(pNewSlayer->getName());
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pFromCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// NPC object handler
//////////////////////////////////////////////////////////////////////////////
void Restore::execute(NPC* pNPC, Creature* pFromCreature)

{
    __BEGIN_TRY


    Assert(pNPC != NULL);
    Assert(pFromCreature != NULL);

    try {
        Zone* pZone = pNPC->getZone();
        Assert(pZone != NULL);

        // Only a Vampire can be targeted.
        if (!pFromCreature->isVampire()) {
            return;
        }

        GCMorph1 _GCMorph1;             // To the one being transformed.
        GCMorphSlayer2 _GCMorphSlayer2; // To the onlookers of the transformation.


        bool bHitRoll = true;

        if (bHitRoll) {
            //////////////////////////////////////////////////////////////////////
            // Zone level information of every kind has to be deleted.
            //////////////////////////////////////////////////////////////////////

            // Clear the invite information if a party invite is pending.
            PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
            Assert(pPIIM != NULL);
            pPIIM->cancelInvite(pFromCreature);

            // Delete the party related information.
            int PartyID = pFromCreature->getPartyID();
            if (PartyID != 0) {
                // Delete it from the local manager first.
                LocalPartyManager* pLPM = pZone->getLocalPartyManager();
                Assert(pLPM != NULL);
                pLPM->deletePartyMember(PartyID, pFromCreature);

                // Delete it from the global manager too.
                deleteAllPartyInfo(pFromCreature);
            }

            // Cancel the trade information if a trade was in progress.
            TradeManager* pTM = pZone->getTradeManager();
            Assert(pTM != NULL);
            pTM->cancelTrade(pFromCreature);

            //////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////

            Slayer* pNewSlayer = new Slayer;
            Vampire* pVampire = dynamic_cast<Vampire*>(pFromCreature);

            // Delete any blood drain information still left in the database.
            defaultEffectSaveRepository().deleteCreatureEffect(CREATURE_EFFECT_BLOOD_DRAIN, pFromCreature->getName());

            pNewSlayer->setName(pFromCreature->getName());
            pNewSlayer->setPlayer(pFromCreature->getPlayer());
            pNewSlayer->load();
            // load() takes an object id from the zone it loads in, so set it again.
            pNewSlayer->setZone(pZone);
            pNewSlayer->setObjectID(pFromCreature->getObjectID());
            pNewSlayer->setMoveMode(Creature::MOVE_MODE_WALKING);

            ZoneCoord_t x = pFromCreature->getX();
            ZoneCoord_t y = pFromCreature->getY();
            Dir_t dir = pFromCreature->getDir();

            pNewSlayer->setXYDir(x, y, dir);

            // Update the player pointer in the creature and the creature pointer in the player.
            Player* pFromPlayer = pFromCreature->getPlayer();
            GamePlayer* pFromGamePlayer = dynamic_cast<GamePlayer*>(pFromPlayer);
            pFromGamePlayer->setCreature(pNewSlayer);

            // pFromCreature, the original Vampire object, is about to be deleted,
            // so the value held in PCFinder would become garbage.
            // Therefore remove the Vampire pointer and add the new Slayer pointer.
            de::gameContext().playerCreatures().deleteCreature(pFromCreature->getName());
            de::gameContext().playerCreatures().addCreature(pNewSlayer);

            // Remove it from the guild's list of currently connected members.
            if (pVampire->getGuildID() != 0) {
                Guild* pGuild = de::gameContext().guilds().getGuild(pVampire->getGuildID());
                if (pGuild != NULL)
                    pGuild->deleteCurrentMember(pVampire->getName());
                else
                    filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pVampire->getGuildID(),
                            pVampire->getName().c_str());
            }

            // Swap the inventory.
            Inventory* pInventory = pVampire->getInventory();
            pNewSlayer->setInventory(pInventory);
            pVampire->setInventory(NULL);

            // Swap the stash
            pNewSlayer->deleteStash();
            pNewSlayer->setStash(pVampire->getStash());
            pNewSlayer->setStashNum(pVampire->getStashNum());
            pNewSlayer->setStashStatus(false);
            pVampire->setStash(NULL);

            // Swap the flag set
            pNewSlayer->deleteFlagSet();
            pNewSlayer->setFlagSet(pVampire->getFlagSet());
            pVampire->setFlagSet(NULL);

            Item* pItem = NULL;
            _TPOINT point;

            // Move the worn items into the inventory or onto the ground.
            for (int part = 0; part < (int)Vampire::VAMPIRE_WEAR_MAX; part++) {
                pItem = pVampire->getWearItem((Vampire::WearPart)part);
                if (pItem != NULL) {
                    // Remove it from the gear first.
                    if (isTwohandWeapon(pItem)) {
                        Assert(((Vampire::WearPart)part == Vampire::WEAR_RIGHTHAND) ||
                               ((Vampire::WearPart)part == Vampire::WEAR_LEFTHAND));
                        Assert(pVampire->getWearItem(Vampire::WEAR_RIGHTHAND) ==
                               pVampire->getWearItem(Vampire::WEAR_LEFTHAND));
                        // Two-handed item.
                        pVampire->deleteWearItem(Vampire::WEAR_RIGHTHAND);
                        pVampire->deleteWearItem(Vampire::WEAR_LEFTHAND);
                    } else {
                        pVampire->deleteWearItem((Vampire::WearPart)part);
                    }

                    // If the inventory has room, add it there.
                    if (pInventory->getEmptySlot(pItem, point)) {
                        pInventory->addItem(point.x, point.y, pItem);
                        pItem->save(pNewSlayer->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
                    } else if (pItem->isTimeLimitItem()) {
                        pVampire->deleteItemByMorph(pItem);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    }
                    // Drop it on the ground when there is no room.
                    else {
                        ZoneCoord_t ZoneX = pVampire->getX();
                        ZoneCoord_t ZoneY = pVampire->getY();

                        TPOINT pt;

                        pt = pZone->addItem(pItem, ZoneX, ZoneY);

                        if (pt.x != -1) {
                            pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                            // Write an ItemTraceLog entry
                            if (pItem != NULL && pItem->isTraceItem()) {
                                char zoneName[15];
                                snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                                remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                            }
                        } else {
                            // Write an ItemTraceLog entry
                            if (pItem != NULL && pItem->isTraceItem()) {
                                remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                            }

                            pItem->destroy();
                            SAFE_DELETE(pItem);
                        }
                    }
                }
            }

            pItem = pVampire->getExtraInventorySlotItem();
            if (pItem != NULL) {
                pVampire->deleteItemFromExtraInventorySlot();

                // If the inventory has room, add it there.
                if (pInventory->getEmptySlot(pItem, point)) {
                    pInventory->addItem(point.x, point.y, pItem);
                    pItem->save(pNewSlayer->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
                } else if (pItem->isTimeLimitItem()) {
                    pVampire->deleteItemByMorph(pItem);

                    pItem->destroy();
                    SAFE_DELETE(pItem);
                }
                // Drop it on the ground when there is no room.
                else {
                    TPOINT pt;
                    ZoneCoord_t ZoneX = pVampire->getX();
                    ZoneCoord_t ZoneY = pVampire->getY();

                    pt = pZone->addItem(pItem, ZoneX, ZoneY);

                    if (pt.x != -1) {
                        pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                        // Write an ItemTraceLog entry
                        if (pItem != NULL && pItem->isTraceItem()) {
                            char zoneName[15];
                            snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                            remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                        }
                    } else {
                        // Write an ItemTraceLog entry
                        if (pItem != NULL && pItem->isTraceItem()) {
                            remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                        }

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    }
                }
            }

            pNewSlayer->loadTimeLimitItem();

            // The money the Vampire held does not carry over to the Slayer:
            // the new Slayer starts with none, in hand or in the stash.
            pNewSlayer->setGoldEx(0);
            pNewSlayer->setStashGoldEx(0);


            // Send the skill information.
            pNewSlayer->sendSlayerSkillInfo();

            _GCMorph1.setPCInfo2(pNewSlayer->getSlayerInfo2());
            _GCMorph1.setInventoryInfo(pNewSlayer->getInventoryInfo());
            _GCMorph1.setGearInfo(pNewSlayer->getGearInfo());
            _GCMorph1.setExtraInfo(pNewSlayer->getExtraInfo());

            _GCMorphSlayer2.setSlayerInfo(pNewSlayer->getSlayerInfo3());

            pFromPlayer->sendPacket(&_GCMorph1);


            // Delete the old vampire from the tile and the zone and add the new slayer,
            // on the nearest tile that will take it.
            pZone->replacePC(pFromCreature, pNewSlayer, x, y, dir, true);


            GCDeleteObject _GCDeleteObject;
            _GCDeleteObject.setObjectID(pFromCreature->getObjectID());
            pZone->broadcastPacket(x, y, &_GCDeleteObject, pNewSlayer);
            pZone->broadcastPacket(x, y, &_GCMorphSlayer2, pNewSlayer);

            pNewSlayer->tinysave("Race='SLAYER'");
            SAFE_DELETE(pFromCreature);


            // Update the field of view.
            pZone->updateHiddenScan(pNewSlayer);

            EffectRestore* pEffectRestore = new EffectRestore(pNewSlayer);
            pEffectRestore->setDeadline(60 * 60 * 24 * 7 * 10); // 7 days
            pNewSlayer->addEffect(pEffectRestore);
            pNewSlayer->setFlag(Effect::EFFECT_CLASS_RESTORE);
            pEffectRestore->create(pNewSlayer->getName());
        } else {
            executeSkillFailNormal(pNPC, getSkillType(), pFromCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pNPC, getSkillType());
    }


    __END_CATCH
}

Restore g_Restore;
