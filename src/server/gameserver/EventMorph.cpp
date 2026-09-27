//////////////////////////////////////////////////////////////////////////////
// Filename    : EventMorph.cpp
// Written by  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EventMorph.h"

#include <stdio.h>

#include <fstream>

#include "CreatureUtil.h"
#include "GCMorph1.h"
#include "GCMorphVampire2.h"
#include "GCUpdateInfo.h"
#include "GSGuildMemberLogOn.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "IncomingPlayerManager.h"
#include "ItemUtil.h"
#include "PCFinder.h"
#include "PacketUtil.h"
#include "Party.h"
#include "RelicUtil.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#include "repository/SessionRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class EventMorph member methods
//////////////////////////////////////////////////////////////////////////////

EventMorph::EventMorph(GamePlayer* pGamePlayer)

    : Event(pGamePlayer){__BEGIN_TRY __END_CATCH}

      EventMorph::~EventMorph()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void EventMorph::activate()

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(m_pGamePlayer != NULL);

    Creature* pFromCreature = m_pGamePlayer->getCreature();
    Assert(pFromCreature->isSlayer());

    if (m_pGamePlayer->getPlayerStatus() != GPS_NORMAL) {
        // There was a bug where the player status was WAITING_FOR_CG_READY, morph
        // activated, and the code below that deletes the creature from the zone
        // threw an error and killed the server. It is not known exactly how an
        // event can activate in the CG_READY state, but note that making
        // GamePlayer's EventManager run only while GPS_NORMAL would stop
        // Resurrect from working. Checking inside GamePlayer is awkward, so
        // the check is done here instead.
        StringStream msg;
        msg << "EventMorph::activate() : GamePlayer status is not GPS_NORMAL." << "PlayerID[" << m_pGamePlayer->getID()
            << "]" << "CreatureName[" << pFromCreature->getName() << "]";

        filelog("EventMorphError.log", "%s", msg.toString().c_str());
        return;
    }

    pFromCreature->removeFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN);
    Zone* pZone = pFromCreature->getZone();

    // No morph while the Restore effect is active.
    if (pFromCreature->isFlag(Effect::EFFECT_CLASS_RESTORE)) {
        return;
    }

    dropRelicToZone(pFromCreature);
    dropFlagToZone(pFromCreature);
    dropSweeperToZone(pFromCreature);

    //////////////////////////////////////////////////////////////////////
    // Zone-level information of various kinds has to be removed.
    //////////////////////////////////////////////////////////////////////

    // Remove the party invitation info if an invite is pending.
    PartyInviteInfoManager* pPIIM = pZone->getPartyInviteInfoManager();
    Assert(pPIIM != NULL);
    pPIIM->cancelInvite(pFromCreature);

    // Remove the party related information.
    uint PartyID = pFromCreature->getPartyID();
    if (PartyID != 0) {
        // First remove it locally...
        LocalPartyManager* pLPM = pZone->getLocalPartyManager();
        Assert(pLPM != NULL);
        pLPM->deletePartyMember(PartyID, pFromCreature);

        // Then remove it globally as well.
        deleteAllPartyInfo(pFromCreature);
    }

    // Remove the trade information if a trade was in progress.
    TradeManager* pTM = pZone->getTradeManager();
    Assert(pTM != NULL);
    pTM->cancelTrade(pFromCreature);

    //////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////

    Vampire* pVampire = new Vampire();

    GCMorph1 gcEventMorph1;               // To the one who morphs..
    GCMorphVampire2 gcEventMorphVampire2; // To the onlookers..

    pVampire->setName(pFromCreature->getName());

    ObjectID_t fromObjectID = pFromCreature->getObjectID();
    pVampire->setObjectID(fromObjectID);

    Player* pPlayer = pFromCreature->getPlayer();
    dynamic_cast<GamePlayer*>(pPlayer)->setCreature(pVampire);
    pVampire->setPlayer(pPlayer);
    pVampire->setZone(pZone);
    pVampire->load();

    Coord_t x = pFromCreature->getX(), y = pFromCreature->getY();
    Dir_t dir = pFromCreature->getDir();
    pVampire->setXYDir(x, y, dir);
    pVampire->setMoveMode(pFromCreature->getMoveMode());

    // slayer to vampire
    Slayer* pSlayer = dynamic_cast<Slayer*>(pFromCreature);

    // The Creature pointer changes when morphing into a vampire, so the
    // pointer registered before becomes useless.
    // The new Creature pointer has to be registered instead.
    de::gameContext().playerCreatures().deleteCreature(pFromCreature->getName());
    de::gameContext().playerCreatures().addCreature(pVampire);

    // Remove from the guild's currently-connected list.
    if (pSlayer->getGuildID() != 99) {
        Guild* pGuild = de::gameContext().guilds().getGuild(pSlayer->getGuildID());
        if (pGuild != NULL) {
            pGuild->deleteCurrentMember(pSlayer->getName());

            GSGuildMemberLogOn gsGuildMemberLogOn;
            gsGuildMemberLogOn.setGuildID(pGuild->getID());
            gsGuildMemberLogOn.setName(pSlayer->getName());
            gsGuildMemberLogOn.setLogOn(false);

            de::gameContext().sharedServer().sendPacket(&gsGuildMemberLogOn);

            // Update the DB.
            defaultSessionRepository().markGuildMemberLoggedOff(pSlayer->getName());
        } else
            filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n", (int)pSlayer->getGuildID(),
                    pSlayer->getName().c_str());
    }

    // Swap the inventory.
    Inventory* pInventory = pSlayer->getInventory();
    pVampire->setInventory(pInventory);
    pSlayer->setInventory(NULL);

    // Swap the stash
    pVampire->deleteStash();                 // Delete the previous object...
    pVampire->setStash(pSlayer->getStash()); // then take over the slayer's
    pVampire->setStashNum(pSlayer->getStashNum());
    pVampire->setStashStatus(false); // Reset the OID assignment state to false...
    pSlayer->setStash(NULL);         // NULL out the slayer's to avoid a pointer error...


    // Swap the flag set
    pVampire->deleteFlagSet();
    pVampire->setFlagSet(pSlayer->getFlagSet());
    pSlayer->setFlagSet(NULL);

    Item* pItem = NULL;
    _TPOINT point;
    // From the gear into the inventory..
    for (int part = 0; part < (int)Slayer::WEAR_MAX; part++) {
        pItem = pSlayer->getWearItem((Slayer::WearPart)part);
        if (pItem) {
            if (isTwohandWeapon(pItem)) {
                Assert(((Slayer::WearPart)part == Slayer::WEAR_RIGHTHAND) ||
                       ((Slayer::WearPart)part == Slayer::WEAR_LEFTHAND));
                Assert(pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND) == pSlayer->getWearItem(Slayer::WEAR_LEFTHAND));
                // Two-handed item.
                pSlayer->deleteWearItem(Slayer::WEAR_RIGHTHAND);
                pSlayer->deleteWearItem(Slayer::WEAR_LEFTHAND);
            } else {
                pSlayer->deleteWearItem((Slayer::WearPart)part);
            }

            if (pInventory->getEmptySlot(pItem, point)) {
                // If there is a free slot in the inventory..
                // add it to the inventory
                pInventory->addItem(point.x, point.y, pItem);
                pItem->save(pVampire->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
            } else if (pItem->isTimeLimitItem()) {
                pSlayer->deleteItemByMorph(pItem);

                pItem->destroy();
                SAFE_DELETE(pItem);
            } else {
                TPOINT pt;
                ZoneCoord_t ZoneX = pSlayer->getX();
                ZoneCoord_t ZoneY = pSlayer->getY();

                // Drop it into the zone.
                pt = pZone->addItem(pItem, ZoneX, ZoneY);

                if (pt.x != -1) {
                    pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                    // Leave an ItemTraceLog.
                    if (pItem != NULL && pItem->isTraceItem()) {
                        char zoneName[15];
                        snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                        remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                        remainTraceLogNew(pItem, pFromCreature->getName(), ITL_DROP, ITLD_MOVE, pZone->getZoneID(),
                                          pt.x, pt.y);
                    }
                } else {
                    // Leave an ItemTraceLog.
                    if (pItem != NULL && pItem->isTraceItem()) {
                        remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                        remainTraceLogNew(pItem, pFromCreature->getName(), ITL_ETC, ITLD_DELETE);
                    }
                    pItem->destroy();
                    SAFE_DELETE(pItem);
                }
            }
        }
    }
    // From the ExtraInventorySlot into the inventory..
    pItem = pSlayer->getExtraInventorySlotItem();
    if (pItem) {
        pSlayer->deleteItemFromExtraInventorySlot();

        if (pInventory->getEmptySlot(pItem, point)) {
            pInventory->addItem(point.x, point.y, pItem);
            pItem->save(pVampire->getName(), STORAGE_INVENTORY, 0, point.x, point.y);
        } else if (pItem->isTimeLimitItem()) {
            pSlayer->deleteItemByMorph(pItem);

            pItem->destroy();
            SAFE_DELETE(pItem);
        } else {
            TPOINT pt;
            ZoneCoord_t ZoneX = pSlayer->getX();
            ZoneCoord_t ZoneY = pSlayer->getY();

            pt = pZone->addItem(pItem, ZoneX, ZoneY);

            if (pt.x != -1) {
                pItem->save("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y);

                // Leave an ItemTraceLog.
                if (pItem != NULL && pItem->isTraceItem()) {
                    char zoneName[15];
                    snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                    remainTraceLog(pItem, pFromCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
                    remainTraceLogNew(pItem, pFromCreature->getName(), ITL_DROP, ITLD_MOVE, pZone->getZoneID(), pt.x,
                                      pt.y);
                }
            } else {
                // Leave an ItemTraceLog.
                if (pItem != NULL && pItem->isTraceItem()) {
                    remainTraceLog(pItem, pFromCreature->getName(), "GOD", ITEM_LOG_DELETE, DETAIL_DROP);
                    remainTraceLogNew(pItem, pFromCreature->getName(), ITL_DROP, ITLD_DELETE);
                }
                pItem->destroy();
                SAFE_DELETE(pItem);
            }
        }
    }

    if (pSlayer->hasRideMotorcycle()) {
        pSlayer->getOffMotorcycle();
    }

    pVampire->loadTimeLimitItem();

    // Gold is reset when the character becomes a Vampire.
    // pVampire->setGoldEx(pSlayer->getGold());
    pVampire->setGoldEx(0);
    pVampire->setStashGoldEx(0);

    // set packet data
    gcEventMorph1.setPCInfo2(pVampire->getVampireInfo2());
    gcEventMorph1.setInventoryInfo(pVampire->getInventoryInfo());
    gcEventMorph1.setGearInfo(pVampire->getGearInfo());
    gcEventMorph1.setExtraInfo(pVampire->getExtraInfo());

    gcEventMorphVampire2.setVampireInfo(pVampire->getVampireInfo3());

    if (pFromCreature->isPC()) {
        Player* pPlayer = pFromCreature->getPlayer();
        pPlayer->sendPacket(&gcEventMorph1);
    }

    pZone->broadcastPacket(x, y, &gcEventMorphVampire2, pFromCreature);

    // Take the slayer off its tile and put the vampire on that same tile,
    // without applying the tile's effects or activating a portal on it.
    pZone->replacePC(pFromCreature, pVampire, x, y, dir, false, false, false);

    // Update the field of view.
    pZone->updateHiddenScan(pVampire);

    // Vampire skills
    pVampire->sendVampireSkillInfo();

    m_pTargetCreature = NULL;

    // Record the change to vampire in the Slayer table.
    pSlayer->tinysave("Race='VAMPIRE'");

    // Move the character to the vampire town.
    uint ZoneNum = 1003;

    ZoneCoord_t ZoneX = 62;
    ZoneCoord_t ZoneY = 64;

    Assert((int)ZoneX < 256);
    Assert((int)ZoneY < 256);

    Assert(pVampire->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pVampire->getPlayer());
    //	Zone* pZone = pVampire->getZone();

    //--------------------------------------------------------------------------------
    // Find out which server and which zone group the destination zone belongs to.
    //--------------------------------------------------------------------------------
    ZoneInfo* pZoneInfo;
    try {
        pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZoneNum);
    } catch (NoSuchElementException&) {
        cerr << "Critical Error : the zone id set on the portal is wrong, or ZoneInfoManager has no such zone." << endl;
        throw Error("Critical Error : the zone id set on the portal is wrong, or ZoneInfoManager has no such zone.");
    }

    ZoneGroup* pZoneGroup;
    try {
        pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(pZoneInfo->getZoneGroupID());
    } catch (NoSuchElementException&) {
        cerr << "Critical Error : only one game server is supported.." << endl;

        // Only one server is supported for now, so bail out.
        throw Error("Critical Error : only one game server is supported");
    }

    //--------------------------------------------------------------------------------
    // First delete the PC from the previous zone and move the player from the ZPM to the IPM.
    //--------------------------------------------------------------------------------
    try {
        // Now delete the PC from the zone.
        //
        // *CAUTION*
        //
        // pVampire's coordinates must match the tile pVampire actually stands on.
        // So the coordinates have to be set correctly before calling this method.
        //
        pZone->deleteCreature(pVampire, pVampire->getX(), pVampire->getY());

        // Delete the player from the zone group's ZPM.
        // pZone->getZoneGroup()->getZonePlayerManager()->deletePlayer_NOBLOCKED(pGamePlayer);
        // pZone->getZoneGroup()->getZonePlayerManager()->deletePlayer_NOBLOCKED(pGamePlayer->getSocket()->getSOCKET());
        pZone->getZoneGroup()->getZonePlayerManager()->deletePlayer(pGamePlayer->getSocket()->getSOCKET());

        //--------------------------------------------------
        // The creature's new coordinates are the portal's arrival point.
        //--------------------------------------------------
        // pVampire->setXY(ZoneX, ZoneY);
        // pVampire->setZone(NULL);

        // Move the player to the IPM.
        // g_pIncomingPlayerManager->addPlayer(pGamePlayer);
        // g_pIncomingPlayerManager->pushPlayer(pGamePlayer);
        pZone->getZoneGroup()->getZonePlayerManager()->pushOutPlayer(pGamePlayer);

    } catch (NoSuchElementException& nsee) {
        cerr << nsee.toString() << endl;
        throw Error(nsee.toString());
    }

    // Assign the zone to the creature, so that an OID can be allocated.
    Zone* pNewZone = pZoneGroup->getZone(ZoneNum);
    Assert(pNewZone != NULL);

    // pVampire->setZone(pZone);
    //  Set the zone to move to.
    pVampire->setNewZone(pNewZone);
    pVampire->setNewXY(ZoneX, ZoneY);


    // Store the creature's information.
    pVampire->setZone(pNewZone);
    pVampire->setXY(ZoneX, ZoneY);

    pVampire->save();

    pVampire->setZone(pZone);
    pVampire->setXY(x, y);

    // Allocate OIDs for the creature itself and the items it owns.
    // pVampire->registerObject();

    //--------------------------------------------------
    // change player status
    //--------------------------------------------------
    pGamePlayer->setPlayerStatus(GPS_WAITING_FOR_CG_READY);


    // What happens to the running EffectManager if this is deleted now?
    //----------------------------------

    SAFE_DELETE(pFromCreature);

    __END_DEBUG
    __END_CATCH
}

string EventMorph::toString() const

{
    StringStream msg;
    msg << "EventMorph(" << ")";
    return msg.toString();
}
