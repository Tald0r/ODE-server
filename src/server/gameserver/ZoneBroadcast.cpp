//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneBroadcast.cpp
// Description	: Zone packet broadcast: sending zone events to the players who can see them.
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>

#include "Assert.h"
#include "BloodBibleBonusManager.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "Creature.h"
#include "DarkLightInfo.h"
#include "DefaultOptionSetInfo.h"
#include "EffectDarkness.h"
#include "EffectDecayCorpse.h"
#include "EffectDecayItem.h"
#include "EffectLoaderManager.h"
#include "EffectManager.h"
#include "EffectSchedule.h"
#include "EffectVampirePortal.h"
#include "FlagSet.h"
#include "GamePlayer.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "LevelWarZoneInfoManager.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "NPCManager.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "Party.h"
#include "PaySystem.h"
#include "Player.h"
#include "Properties.h"
#include "QuestManager.h"
#include "RegenZoneManager.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "VariableManager.h"
#include "VisionInfo.h"
#include "War.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "repository/ComebackEventRepository.h"
#include "repository/MessageRepository.h"
#include "repository/ZoneInfoRepository.h"
// #include "EffectRevealer.h"
#include "EffectAddItem.h"
#include "EffectAddItemToCorpse.h"
#include "EffectDeleteItem.h"
#include "EffectGnomesWhisper.h"
#include "EffectHasBloodBible.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectObservingEye.h"
#include "EffectRelicTable.h"
#include "EffectSanctuary.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EffectSlayerRelic.h"
#include "EffectTransportItem.h"
#include "EffectTransportItemToCorpse.h"
#include "EffectVampireRelic.h"
// #include "EffectDropBloodBible.h"
#include "EffectContinualGroundAttack.h"
#include "EffectHasCastleSymbol.h"
#include "EffectPKZoneRegen.h"
#include "EventTransport.h"
#include "GCAddBat.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddInstalledMineToZone.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCAddVampirePortal.h"
#include "GCAddWolf.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCDropItemToZone.h"
#include "GCFastMove.h"
#include "GCHolyLandBonusInfo.h"
#include "GCKnockBack.h"
#include "GCMineExplosionOK1.h"
#include "GCMineExplosionOK2.h"
#include "GCModifyInformation.h"
#include "GCMove.h"
#include "GCMoveError.h"
#include "GCMoveOK.h"
#include "GCMyStoreInfo.h"
#include "GCNPCInfo.h"
#include "GCNoticeEvent.h"
#include "GCRegenZoneStatus.h"
#include "GCRemoveEffect.h"
#include "GCSetPosition.h"
#include "GCSweeperBonusInfo.h"
#include "GCSystemMessage.h"
#include "GCUnburrowFail.h"
#include "GCUnburrowOK.h"
#include "GCUnionOfferList.h"
#include "GCUntransformFail.h"
#include "GCUntransformOK.h"
// #include "CGItemNameInfoList.h"

#include "DynamicZone.h"
#include "EffectCastingTrap.h"
#include "GDRLairManager.h"
#include "GGCommand.h"
#include "GQuestManager.h"
#include "GameServerInfoManager.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "LevelWarManager.h"
#include "LoginServerManager.h"
#include "NicknameBook.h"
#include "Profile.h"
#include "ResurrectLocationManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Store.h"
#include "ZoneInternal.h"
#include "item/Motorcycle.h"


#ifdef __PROFILE_BROADCAST__
#define __BEGIN_PROFILE_ZONE(name) beginProfileEx(name);
#define __END_PROFILE_ZONE(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_ZONE(name) ((void)0);
#define __END_PROFILE_ZONE(name) ((void)0);
#endif


#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif

//--------------------------------------------------------------------------------
//
// broadcast packet
//
// Send the given packet to every PC in the zone except owner.
//
//--------------------------------------------------------------------------------
void Zone::broadcastPacket(Packet* pPacket, Creature* owner)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_PCMANAGER")
    m_pPCManager->broadcastPacket(pPacket, owner);
    __END_PROFILE_ZONE("Z_BC_PCMANAGER")

    __END_CATCH
}

void Zone::broadcastDarkLightPacket(Packet* pPacket1, Packet* pPacket2, Creature* owner)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_DARKLIGHT")
    m_pPCManager->broadcastDarkLightPacket(pPacket1, pPacket2, owner);
    __END_PROFILE_ZONE("Z_BC_DARKLIGHT")

    __END_CATCH
}

//--------------------------------------------------------------------
//
// Broadcast a chat message. Different races cannot see each other's chat.
// A packet sent by a vampire arrives with isVampire true.
//
//--------------------------------------------------------------------
void Zone::broadcastSayPacket(ZoneCoord_t cx, ZoneCoord_t cy, Packet* pPacket, Creature* owner, bool isVampire)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_SAY")

    ZoneCoord_t ix = 0;
    ZoneCoord_t iy = 0;
    ZoneCoord_t endx = 0;
    ZoneCoord_t endy = 0;

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
#endif
    pPacket->writeHeaderNBody(outputStream);

    //-------------------------------------------------------------------
    // Initialize the loop variables.
    //
    // When Plus is true, the packet is sent Range tiles further out.
    // This shows the result of an area spell properly.
    //
    // *NOTE
    // - An optimization would add a PLUS_SIGHT value to VisionInfo and compute with it.
    //-------------------------------------------------------------------
    endx = min(m_Width - 1, cx + maxViewportWidth + 1);
    endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);

    for (ix = max(0, cx - maxViewportWidth - 1); ix <= endx; ix++) {
        for (iy = max(0, cy - maxViewportUpperHeight - 1); iy <= endy; iy++) {
            Tile& rTile = m_pTiles[ix][iy]; // by sigi. 2002.5.8

            // Only when the tile holds a creature.
            if (rTile.hasCreature()) {
                const forward_list<Object*>& objectList = rTile.getObjectList();

                for (forward_list<Object*>::const_iterator itr = objectList.begin();
                     itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    Creature* pCreature = dynamic_cast<Creature*>(*itr);
                    Assert(pCreature != NULL);

                    // A PC that is not owner and can see (x,y).
                    if ((pCreature->isPC() && pCreature != owner && pCreature->getVisionState(cx, cy) >= IN_SIGHT) ||
                        (pCreature->isPC() && pCreature != owner)) {
                        // A hidden creature that acts is not shown; acting should unburrow it.
                        // A packet sent by a vampire arrives with isVampire true.
                        if (owner != NULL) {
                            // Fetch the ObservingEye effect from the creature if it has one.
                            EffectObservingEye* pEffectObservingEye = NULL;
                            if (pCreature->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
                                pEffectObservingEye = dynamic_cast<EffectObservingEye*>(
                                    pCreature->findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
                            }

                            if (!owner->isFlag(Effect::EFFECT_CLASS_GHOST) &&
                                (!owner->isFlag(Effect::EFFECT_CLASS_HIDE) || pCreature->isVampire() ||
                                 pCreature->isFlag(
                                     Effect::EFFECT_CLASS_DETECT_HIDDEN)) // || ( pEffectRevealer != NULL &&
                                                                          // pEffectRevealer->canSeeHide(owner) ) )
                                && (!owner->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) || pCreature->isVampire() ||
                                    pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) ||
                                    (pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(owner))) &&
                                (!owner->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
                                 pCreature->isFlag(
                                     Effect::EFFECT_CLASS_DETECT_INVISIBILITY)) // || ( pEffectRevealer != NULL &&
                                                                                // pEffectRevealer->canSeeSniping(owner)
                                                                                // ) )
                                && ((isVampire && pCreature->isVampire()) || (!isVampire && pCreature->isSlayer()) ||
                                    pCreature->isOusters())) {
                                pCreature->getPlayer()->sendStream(&outputStream);
                            }
                        } else {
                            pCreature->getPlayer()->sendStream(&outputStream);
                        }
                    }
                }
            }
        }
    }

    __END_PROFILE_ZONE("Z_BC_SAY")

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// broadcast packet
//
// Broadcast the packet to every PC except owner that can see the event at (x,y).
//
// *CAUTION*
//
// ZoneCoord_t is an unsigned char; watch for overflow and underflow.
//
//--------------------------------------------------------------------------------
void Zone::broadcastPacket(ZoneCoord_t cx, ZoneCoord_t cy, Packet* pPacket, Creature* owner, bool Plus, Range_t Range)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_NORMAL")

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
#endif
    pPacket->writeHeaderNBody(outputStream);

    ZoneCoord_t ix = 0;
    ZoneCoord_t iy = 0;
    ZoneCoord_t endx = 0;
    ZoneCoord_t endy = 0;

    //-------------------------------------------------------------------
    // Initialize the loop variables.
    //
    // When Plus is true, the packet is sent Range tiles further out.
    // This shows the result of an area spell properly.
    //
    // *NOTE
    // - An optimization would add a PLUS_SIGHT value to VisionInfo and compute with it.
    //-------------------------------------------------------------------
    endx = min(m_Width - 1, cx + maxViewportWidth + 1 + Range);
    endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1 + Range);

    for (ix = max(0, cx - maxViewportWidth - 1 - Range); ix <= endx; ix++) {
        for (iy = max(0, cy - maxViewportUpperHeight - 1 - Range); iy <= endy; iy++) {
            // When (cx,cy) cannot be seen from (ix,iy).
            if (VisionInfoManager::getVisionState(ix, iy, cx, cy) == OUT_OF_SIGHT && !Plus)
                continue;
            Tile& rTile = m_pTiles[ix][iy]; // by sigi.2002.5.8

            // Only when the tile holds a creature.
            if (rTile.hasCreature()) {
                const forward_list<Object*>& objectList = rTile.getObjectList();

                for (forward_list<Object*>::const_iterator itr = objectList.begin();
                     itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    Creature* pCreature = dynamic_cast<Creature*>(*itr);
                    Assert(pCreature != NULL);

                    // by sigi. 2002.5.14
                    if (pCreature->isPC() && pCreature != owner)
                    // Already checked above.
                    //						&& (pCreature->getVisionState(cx,cy) >= IN_SIGHT || Plus))
                    {
                        // A hidden creature that acts is not shown; acting should unburrow it.
                        if (owner != NULL) {
                            // The canSee function makes this decision.
                            if (canSee(pCreature, owner)) {
                                pCreature->getPlayer()->sendStream(&outputStream);
                            }
                        } else {
                            pCreature->getPlayer()->sendStream(&outputStream);
                        }
                    }
                }
            }
        }
    }

    __END_PROFILE_ZONE("Z_BC_NORMAL")

    __END_CATCH
}

void Zone::broadcastLevelWarBonusPacket(Packet* pPacket, Creature* owner)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_PCMANAGER")
    m_pPCManager->broadcastLevelWarBonusPacket(pPacket, owner);
    __END_PROFILE_ZONE("Z_BC_PCMANAGER")

    __END_CATCH
}

//--------------------------------------------------------------------------------
// broadcast packet
// Broadcast the packet to every PC except owner that can see the event at
// (x1,y1) or (x2,y2).
// This is the broadcastPacket variant for tile skills.
// *CAUTION*
// ZoneCoord_t is an unsigned char; watch for overflow and underflow.
//--------------------------------------------------------------------------------
list<Creature*> Zone::broadcastSkillPacket(ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                                           Packet* pPacket, list<Creature*> creatureList, bool bConcernDarkness)

{
    __BEGIN_TRY

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
#endif
    pPacket->writeHeaderNBody(outputStream);

    list<Creature*> cList;

    __BEGIN_PROFILE_ZONE("Z_BC_SKILL")

    ZoneCoord_t ix = 0;
    ZoneCoord_t iy = 0;
    ZoneCoord_t endx = 0;
    ZoneCoord_t endy = 0;

    //-------------------------------------------------------------------
    // Initialize the loop variables.
    //
    // When Plus is true, the packet is sent Range tiles further out.
    // This shows the result of an area spell properly.
    //
    // *NOTE
    //
    // - An optimization would add a PLUS_SIGHT value to VisionInfo and compute with it.
    //
    //-------------------------------------------------------------------
    endx = min(m_Width - 1, x1 + maxViewportWidth + 1);
    endy = min(m_Height - 1, y1 + maxViewportLowerHeight + 1);

    for (ix = max(0, x1 - maxViewportWidth - 1); ix <= endx; ix++) {
        for (iy = max(0, y1 - maxViewportUpperHeight - 1); iy <= endy; iy++) {
            Tile& rTile = m_pTiles[ix][iy]; // by sigi.2002.5.8

            // Only when the tile holds a creature.
            if (rTile.hasCreature()) {
                const forward_list<Object*>& objectList = rTile.getObjectList();

                for (forward_list<Object*>::const_iterator itr = objectList.begin();
                     itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    Creature* pCreature = dynamic_cast<Creature*>(*itr);
                    Assert(pCreature != NULL);

                    // A PC not in creatureList that can see (x,y).
                    if (pCreature->isPC()) {
                        // Check whether this creature caused the packet.
                        bool belong = false;
                        for (list<Creature*>::const_iterator itr = creatureList.begin(); itr != creatureList.end();
                             itr++) {
                            if (pCreature == *itr) {
                                belong = true;
                                break;
                            }
                        }

                        if (!belong && pCreature->getVisionState(x1, y1) >= IN_SIGHT &&
                            pCreature->getVisionState(x2, y2) >= IN_SIGHT) {
                            // A hidden creature has no reason to use a skill unseen, so HIDE is not checked.
                            Player* pPlayer = pCreature->getPlayer();
                            pPlayer->sendStream(&outputStream);
                            cList.push_back(pCreature);
                        }
                    }
                }
            }
        }
    }

    // add creature list to cList
    for (list<Creature*>::const_iterator itr = creatureList.begin(); itr != creatureList.end(); itr++) {
        cList.push_back(*itr);
    }

    __END_PROFILE_ZONE("Z_BC_SKILL")

    return cList;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Broadcast the packet to every PC that can see the event at (x,y), except the
// creatures in creatureList.
//
// *NOTE*
// For a continuous tile magic Plus is set true; when Plus is true the range is
// widened by the magic's radius so the area spell is not clipped and is shown
// properly.
//
// *CAUTION*
//
// ZoneCoord_t is an unsigned char; watch for overflow and underflow.
//////////////////////////////////////////////////////////////////////////////
void Zone::broadcastPacket(ZoneCoord_t cx, ZoneCoord_t cy, Packet* pPacket, const list<Creature*>& creatureList,
                           bool Plus, Range_t Range)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_EXCLIST")

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
#endif
    pPacket->writeHeaderNBody(outputStream);

    ZoneCoord_t ix = 0;
    ZoneCoord_t iy = 0;
    ZoneCoord_t endx = 0;
    ZoneCoord_t endy = 0;

    //////////////////////////////////////////////////////////////////////////////
    // Initialize the loop variables.
    //
    // When Plus is true, the packet is sent Range tiles further out.
    // This shows the result of an area spell properly.
    //
    // *NOTE
    // - An optimization would add a PLUS_SIGHT value to VisionInfo and compute with it.
    //////////////////////////////////////////////////////////////////////////////
    endx = min(m_Width - 1, cx + maxViewportWidth + 1 + Range);
    endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1 + Range);

    for (ix = max(0, cx - maxViewportWidth - 1 - Range); ix <= endx; ix++) {
        for (iy = max(0, cy - maxViewportUpperHeight - 1 - Range); iy <= endy; iy++) {
            if (VisionInfoManager::getVisionState(ix, iy, cx, cy) == OUT_OF_SIGHT || Plus)
                continue;
            Tile& rTile = m_pTiles[ix][iy]; // by sigi. 2002.5.8

            // Only when the tile holds a creature.
            if (rTile.hasCreature()) {
                const forward_list<Object*>& objectList = rTile.getObjectList();
                forward_list<Object*>::const_iterator itr = objectList.begin();

                for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    Creature* pCreature = dynamic_cast<Creature*>(*itr);
                    Assert(pCreature != NULL);

                    // A PC not in creatureList that can see (x,y).
                    if (pCreature->isPC()) {
                        bool belong = false;
                        for (list<Creature*>::const_iterator itr = creatureList.begin(); itr != creatureList.end();
                             itr++) {
                            if (pCreature == *itr) {
                                belong = true;
                                break;
                            }
                        } // for

                        if (!belong) {
                            pCreature->getPlayer()->sendStream(&outputStream);
                        } // if
                    } // if

                } // for
            } // if
        } // for
    } // for

    __END_PROFILE_ZONE("Z_BC_EXCLIST")

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// When a PC moves from (x1,y1) to (x2,y2), the PC must be told about everything
// it can newly see, and every other creature that can newly see the PC must be
// told about it.
//
// bSendMove says whether the move packet is sent.
// bKnockback says whether the movement is a normal one or is forced by a
// knockback.
//////////////////////////////////////////////////////////////////////////////
void Zone::movePCBroadcast(Creature* pPC, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                           bool bSendMove, bool bKnockback) {
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_MOVEPC");

    try {
        // This method operates on a PC.
        Assert(pPC->isPC());

        //////////////////////////////////////////////////////////////////////////////
        // Build the GCMove packet for the PC's own movement. When GCMove is sent to
        // the client, (x,y) must be the previous coordinate and dir the facing
        // (movement) direction. That is the current policy.
        //////////////////////////////////////////////////////////////////////////////
        GCMove gcMove;
        if (bSendMove) {
            gcMove.setObjectID(pPC->getObjectID());
            gcMove.setX(x1);
            gcMove.setY(y1);
            gcMove.setDir(pPC->getDir());
        }
        GCKnockBack gcKnockback;
        if (bKnockback) {
            gcKnockback.setObjectID(pPC->getObjectID());
            gcKnockback.setOrigin(x1, y1);
            gcKnockback.setTarget(x2, y2);
        }

#ifdef __USE_ENCRYPTER__
        SocketEncryptOutputStream outputStream(
            NULL, szPacketHeader + (bSendMove ? gcMove.getPacketSize() : gcKnockback.getPacketSize()) + 2);
        outputStream.setEncryptCode(m_EncryptCode);
#else
        SocketOutputStream outputStream(
            NULL, szPacketHeader + (bSendMove ? gcMove.getPacketSize() : gcKnockback.getPacketSize()) + 2);
#endif
        if (bSendMove)
            gcMove.writeHeaderNBody(outputStream);
        else
            gcKnockback.writeHeaderNBody(outputStream);

        //////////////////////////////////////////////////////////////////////////////
        // Build the GCAdd packet for the moving PC's type, for the other PCs that
        // newly see it. By the current policy GCAdd uses the current coordinate.
        //////////////////////////////////////////////////////////////////////////////
        Packet* pGCAddXXX = NULL;

        if (pPC->getCreatureClass() == Creature::CREATURE_CLASS_SLAYER) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

            GCAddSlayer* pGCAddSlayer = new GCAddSlayer;
            makeGCAddSlayer(pGCAddSlayer, pSlayer);
            pGCAddXXX = pGCAddSlayer;
        } else if (pPC->getCreatureClass() == Creature::CREATURE_CLASS_VAMPIRE) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

            // Moving while hidden is not possible, but handle it anyway.
            if (pPC->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                GCAddBurrowingCreature* pGCABC = new GCAddBurrowingCreature();
                pGCABC->setObjectID(pVampire->getObjectID());
                pGCABC->setName(pVampire->getName());
                pGCABC->setX(x2);
                pGCABC->setY(y2);
                pGCAddXXX = pGCABC;
            } else {
                GCAddVampire* pGCAddVampire = new GCAddVampire;
                makeGCAddVampire(pGCAddVampire, pVampire);
                pGCAddXXX = pGCAddVampire;
            }
        } else if (pPC->getCreatureClass() == Creature::CREATURE_CLASS_OUSTERS) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);

            GCAddOusters* pGCAddOusters = new GCAddOusters;
            makeGCAddOusters(pGCAddOusters, pOusters);
            pGCAddXXX = pGCAddOusters;
        }

        //////////////////////////////////////////////////////////////////////////////
        // Because the PC moves, some of the creatures that were watching it can no
        // longer see it. Build the GCDeleteObject packet that will be sent to those
        // creatures.
        //////////////////////////////////////////////////////////////////////////////
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pPC->getObjectID());

        Player* pPlayer = pPC->getPlayer();
        Assert(pPlayer != NULL);


        //////////////////////////////////////////////////////////////////////////////
        // Expand the vision area by 1 on all four sides.
        // The ON_SIGHT area grows with the facing direction.
        //////////////////////////////////////////////////////////////////////////////
        for (ZoneCoord_t ix = max(0, x2 - maxViewportWidth - 1), endx = min(m_Width - 1, x2 + maxViewportWidth + 1);
             ix <= endx; ix++) {
            for (ZoneCoord_t iy = max(0, y2 - maxViewportUpperHeight - 1),
                             endy = min(m_Height - 1, y2 + maxViewportLowerHeight + 1);
                 iy <= endy; iy++) {
                // Iterate over every object on the current tile.
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

                forward_list<Object*>::const_iterator itr = objectList.begin();

                //
                // The if - do~while form is used so that
                // pVisionInfo->getVisionState() is checked only when the tile
                // actually holds an object.
                //
                if (itr != objectList.end()) {
                    // How is I(ix,iy) seen from the previous coordinate P(x1,y1)?
                    VisionState prevVisionState = VisionInfoManager::getVisionState(x1, y1, ix, iy);
                    // How is I(ix,iy) seen from the current coordinate Q(x2,y2)?
                    VisionState curVisionState = VisionInfoManager::getVisionState(x2, y2, ix, iy);

                    do {
                        Assert(*itr != NULL);

                        Object* pDebugObject = *itr;

                        Object::ObjectClass OClass = pDebugObject->getObjectClass();

                        ////////////////////////////////////////////////////////////
                        // Build the GCAddXXX packet matching each object's OBJECT CLASS
                        // and send it to owner.
                        ////////////////////////////////////////////////////////////

                        ////////////////////////////////////////////////////////////
                        // The tile holds a creature.
                        ////////////////////////////////////////////////////////////
                        if (OClass == Object::OBJECT_CLASS_CREATURE) {
                            Creature* pCreature = dynamic_cast<Creature*>(*itr);
                            Assert(pCreature != NULL);

                            if (pCreature == pPC) {
                                // On a knockback the creature is moved by someone else
                                // rather than by its own will, so it must be sent.
                                if (bKnockback) {
                                    pPC->getPlayer()->sendStream(&outputStream);
                                    // Once the knockback has been sent, continue.
                                }

                                // The PC does not need its own move information.
                                continue;
                            }

                            Creature::CreatureClass CClass = pCreature->getCreatureClass();

                            // The if order is Monster > Slayer > Vampire > NPC; guild
                            // buildings and the like may differ.
                            // by sigi. 2002.5.8
                            if (CClass == Creature::CREATURE_CLASS_MONSTER) {
                                Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                                //--------------------------------------------------------------------------------
                                // Send GCAddMonster when the monster was not visible from
                                // the previous coordinate but is visible from the new one.
                                //--------------------------------------------------------------------------------
                                if (prevVisionState == OUT_OF_SIGHT && curVisionState >= IN_SIGHT) {
                                    // by sigi
                                    Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, pPC);

                                    if (pAddMonsterPacket != NULL) {
                                        pPlayer->sendPacket(pAddMonsterPacket);

                                        delete pAddMonsterPacket;
                                    }
                                }

                                // Register the PC as a potential enemy of the monster.
                                VisionState vs = pMonster->getVisionState(x2, y2);

                                // Only aggressive monsters register an enemy.
                                if (vs >= IN_SIGHT && pMonster->getAlignment() == ALIGNMENT_AGGRESSIVE) {
                                    if (isPotentialEnemy(pMonster, pPC)) {
                                        pMonster->addPotentialEnemy(pPC);
                                    }
                                }
                            } else if (CClass == Creature::CREATURE_CLASS_SLAYER) {
                                // When the tile was invisible before and is visible now
                                // and a creature stands on it, the moving PC must be told
                                // about that creature.
                                if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                    // The moving creature sees it only when sniping is
                                    // not set, or, if it is, detect invisibility is set.
                                    // canSee makes this decision.
                                    if (canSee(pPC, pCreature)) {
                                        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                                        //									GCAddSlayer
                                        GCAddSlayer gcAddSlayer;
                                        makeGCAddSlayer(&gcAddSlayer, pSlayer);
                                        pPlayer->sendPacket(&gcAddSlayer);
                                    }
                                }

                                Assert(pCreature->getPlayer() != NULL);

                                //////////////////////////////////////////////////////////////////////////////
                                // GCAddXXX is sent only when Q(x2,y2) lies on the boundary
                                // of this creature's vision rectangle while P(x1,y1) was
                                // outside it, that is, invisible. Otherwise the server would
                                // keep sending GCAddXXX along the vision boundary.
                                //
                                // In summary,
                                //
                                // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                                // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                                //////////////////////////////////////////////////////////////////////////////
                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // Moving from the invisible area straight into the inner
                                // vision area without passing the boundary is impossible.
                                // Only IN_SIGHT remains now.

                                if (canSee(pCreature, pPC)) {
                                    if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                    }
                                }
                            } else if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
                                //////////////////////////////////////////////////////////////////////////////
                                // Only a creature that was invisible from the previous
                                // coordinate and is newly visible from this one gets
                                // GCAddXXX; NEW_SIGHT to NEW_SIGHT does not.
                                //////////////////////////////////////////////////////////////////////////////
                                if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                    if (canSee(pPC, pCreature)) {
                                        if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                                            //										if
                                            //(!pCreature->isFlag(Effect::EFFECT_CLASS_GHOST)
                                            //											&& (pPC->isVampire()
                                            //												||
                                            // pPC->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) )// || ( pEffectRevealer
                                            //!= NULL && pEffectRevealer->canSeeHide( pCreature ) ) )
                                            //											)
                                            {
                                                GCAddBurrowingCreature gcABC;
                                                gcABC.setObjectID(pCreature->getObjectID());
                                                gcABC.setName(pCreature->getName());
                                                gcABC.setX(ix);
                                                gcABC.setY(iy);
                                                pPlayer->sendPacket(&gcABC);
                                            }
                                        } else {
                                            //										if
                                            //(!pCreature->isFlag(Effect::EFFECT_CLASS_GHOST)
                                            //											&&
                                            //(!pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)
                                            //												|| pPC->isVampire()
                                            //												||
                                            // pPC->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) || (
                                            // pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(
                                            // pCreature ) ) )
                                            //											)
                                            {
                                                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                                                //											GCAddVampire
                                                GCAddVampire gcAddVampire;
                                                makeGCAddVampire(&gcAddVampire, pVampire);

                                                pPlayer->sendPacket(&gcAddVampire);
                                            }
                                        }
                                    }
                                }

                                Assert(pCreature->getPlayer() != NULL);

                                //////////////////////////////////////////////////////////////////////////////
                                // GCAddXXX is sent only when Q(x2,y2) lies on the boundary
                                // of this creature's vision rectangle while P(x1,y1) was
                                // outside it, that is, invisible. Otherwise the server would
                                // keep sending GCAddXXX along the vision boundary.
                                //
                                // In summary,
                                //
                                // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                                // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                                //////////////////////////////////////////////////////////////////////////////
                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // Moving from the invisible area straight into the inner
                                // vision area without passing the boundary is impossible.
                                // Only IN_SIGHT remains now.

                                // The other side is a vampire, so this creature's darkness
                                // state does not matter, and neither does Hide.
                                if (canSee(pCreature, pPC)) {
                                    if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                    }
                                }
                            }

                            else if (CClass == Creature::CREATURE_CLASS_OUSTERS) {
                                //////////////////////////////////////////////////////////////////////////////
                                // Only a creature that was invisible from the previous
                                // coordinate and is newly visible from this one gets
                                // GCAddXXX; NEW_SIGHT to NEW_SIGHT does not.
                                //////////////////////////////////////////////////////////////////////////////
                                if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                    if (canSee(pPC, pCreature)) {
                                        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                                        //									GCAddOusters
                                        GCAddOusters gcAddOusters;
                                        makeGCAddOusters(&gcAddOusters, pOusters);
                                        pPlayer->sendPacket(&gcAddOusters);
                                    }
                                }

                                Assert(pCreature->getPlayer() != NULL);

                                //////////////////////////////////////////////////////////////////////////////
                                // GCAddXXX is sent only when Q(x2,y2) lies on the boundary
                                // of this creature's vision rectangle while P(x1,y1) was
                                // outside it, that is, invisible. Otherwise the server would
                                // keep sending GCAddXXX along the vision boundary.
                                //
                                // In summary,
                                //
                                // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                                // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                                //////////////////////////////////////////////////////////////////////////////
                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // Moving from the invisible area straight into the inner
                                // vision area without passing the boundary is impossible.

                                if (canSee(pCreature, pPC)) {
                                    if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                    }
                                }
                            }

                            else if (CClass == Creature::CREATURE_CLASS_NPC) {
                                NPC* pNPC = dynamic_cast<NPC*>(pCreature);

                                //--------------------------------------------------------------------------------
                                //
                                // Send GCAddMonster when the monster was not visible from
                                // the previous coordinate but is visible from the new one.
                                //
                                //--------------------------------------------------------------------------------
                                if (prevVisionState == OUT_OF_SIGHT && curVisionState >= IN_SIGHT) {
                                    GCAddNPC gcAddNPC;
                                    makeGCAddNPC(&gcAddNPC, pNPC);
                                    pPlayer->sendPacket(&gcAddNPC);
                                }
                            } else {
                                throw Error("invalid creature class");
                            }
                        }
                        ////////////////////////////////////////////////////////////
                        // The tile holds an item.
                        ////////////////////////////////////////////////////////////
                        else if (OClass == Object::OBJECT_CLASS_ITEM) {
                            if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                Item* pItem = dynamic_cast<Item*>(*itr);

                                Item::ItemClass IClass = pItem->getItemClass();

                                if (IClass == Item::ITEM_CLASS_CORPSE) {
                                    ItemType_t IType = pItem->getItemType();

                                    if (IType == SLAYER_CORPSE) {
                                        SlayerCorpse* pSlayerCorpse = dynamic_cast<SlayerCorpse*>(pItem);
                                        GCAddSlayerCorpse gcAddSlayerCorpse;
                                        makeGCAddSlayerCorpse(&gcAddSlayerCorpse, pSlayerCorpse);
                                        pPlayer->sendPacket(&gcAddSlayerCorpse);
                                    } else if (IType == VAMPIRE_CORPSE) {
                                        VampireCorpse* pVampireCorpse = dynamic_cast<VampireCorpse*>(pItem);
                                        GCAddVampireCorpse gcAddVampireCorpse;
                                        makeGCAddVampireCorpse(&gcAddVampireCorpse, pVampireCorpse);
                                        pPlayer->sendPacket(&gcAddVampireCorpse);
                                    } else if (IType == OUSTERS_CORPSE) {
                                        OustersCorpse* pOustersCorpse = dynamic_cast<OustersCorpse*>(pItem);
                                        GCAddOustersCorpse gcAddOustersCorpse;
                                        makeGCAddOustersCorpse(&gcAddOustersCorpse, pOustersCorpse);
                                        pPlayer->sendPacket(&gcAddOustersCorpse);
                                    } else if (IType == NPC_CORPSE) {
                                        throw UnsupportedError();
                                    } else if (MONSTER_CORPSE) {
                                        MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                                        GCAddMonsterCorpse gcAddMonsterCorpse;
                                        makeGCAddMonsterCorpse(&gcAddMonsterCorpse, pMonsterCorpse, ix, iy);
                                        pPlayer->sendPacket(&gcAddMonsterCorpse);

                                        sendRelicEffect(pMonsterCorpse, pPlayer);
                                    } else {
                                        Assert(false);
                                    }
                                } else if (pItem->getItemClass() == Item::ITEM_CLASS_MINE &&
                                           pItem->isFlag(Effect::EFFECT_CLASS_INSTALL)) {
                                    if (pPC->isFlag(Effect::EFFECT_CLASS_REVEALER)) {
                                        GCAddInstalledMineToZone gcAddMine;
                                        gcAddMine.setObjectID(pItem->getObjectID());
                                        gcAddMine.setX(ix);
                                        gcAddMine.setY(iy);
                                        gcAddMine.setItemClass(pItem->getItemClass());
                                        gcAddMine.setItemType(pItem->getItemType());
                                        gcAddMine.setOptionType(pItem->getOptionTypeList());
                                        gcAddMine.setDurability(pItem->getDurability());
                                        pPlayer->sendPacket(&gcAddMine);
                                    }
                                } else {
                                    GCAddNewItemToZone gcAddNewItemToZone;
                                    makeGCAddNewItemToZone(&gcAddNewItemToZone, pItem, ix, iy);
                                    pPlayer->sendPacket(&gcAddNewItemToZone);
                                }
                            }
                        }
                        ////////////////////////////////////////////////////////////
                        // The tile holds an effect.
                        ////////////////////////////////////////////////////////////
                        else if (OClass == Object::OBJECT_CLASS_EFFECT) {
                            Effect* pEffect = dynamic_cast<Effect*>(*itr);

                            // Check whether this is a broadcasting effect.
                            if (pEffect->isBroadcastingEffect() && curVisionState >= IN_SIGHT &&
                                prevVisionState == OUT_OF_SIGHT) {
                                if (pEffect->getEffectClass() == Effect::EFFECT_CLASS_VAMPIRE_PORTAL) {
                                    EffectVampirePortal* pEffectVampirePortal =
                                        dynamic_cast<EffectVampirePortal*>(pEffect);
                                    ZONE_COORD zonecoord = pEffectVampirePortal->getZoneCoord();

                                    GCAddVampirePortal gcAddVampirePortal;
                                    gcAddVampirePortal.setObjectID(pEffect->getObjectID());
                                    gcAddVampirePortal.setOwnerID(pEffectVampirePortal->getOwnerID());
                                    gcAddVampirePortal.setX(ix);
                                    gcAddVampirePortal.setY(iy);
                                    gcAddVampirePortal.setTargetZoneID(zonecoord.id);
                                    gcAddVampirePortal.setTargetX(zonecoord.x);
                                    gcAddVampirePortal.setTargetY(zonecoord.y);
                                    gcAddVampirePortal.setDuration(pEffectVampirePortal->getRemainDuration());
                                    gcAddVampirePortal.setCreateFlag(0);

                                    pPlayer->sendPacket(&gcAddVampirePortal);
                                }
                                // by sigi. 2002.6.10
                                else if (pEffect->getEffectClass() == Effect::EFFECT_CLASS_SANCTUARY) {
                                    EffectSanctuary* pEffectSanctuary = dynamic_cast<EffectSanctuary*>(pEffect);

                                    ZoneCoord_t centerX = pEffectSanctuary->getCenterX();
                                    ZoneCoord_t centerY = pEffectSanctuary->getCenterY();

                                    // A sanctuary sends the packet only at its center tile.
                                    if (centerX == ix && centerY == iy) {
                                        GCAddEffectToTile gcAddEffectToTile;

                                        gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                                        gcAddEffectToTile.setXY(ix, iy);
                                        gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                                        gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                                        pPlayer->sendPacket(&gcAddEffectToTile);
                                    }
                                } else {
                                    GCAddEffectToTile gcAddEffectToTile;

                                    gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                                    gcAddEffectToTile.setXY(ix, iy);
                                    gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                                    gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                                    pPlayer->sendPacket(&gcAddEffectToTile);
                                }
                            }
                        }
                        ////////////////////////////////////////////////////////////
                        // The tile holds an obstacle.
                        ////////////////////////////////////////////////////////////
                        else if (OClass == Object::OBJECT_CLASS_OBSTACLE) {
                        }
                        ////////////////////////////////////////////////////////////
                        // The tile holds a portal.
                        ////////////////////////////////////////////////////////////
                        else if (OClass == Object::OBJECT_CLASS_PORTAL) {
                            // darkness
                        } else {
                            throw Error("invalid object class");
                        }

                    } while (++itr != objectList.end()); // by sigi. 2002.5.8
                } // end of the loop over objects
            } // end of the loop over Y coordinates
        } // end of the loop over X coordinates

        SAFE_DELETE(pGCAddXXX);

    } catch (Throwable& t) {
        diagnosticFilelog("Zone_movePCBroadcast.log", "%s", t.toString().c_str());
        throw t;
    }

    __END_PROFILE_ZONE("Z_BC_MOVEPC");

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// moveCreatureBroadcast
//
// Broadcast to the PCs in the surrounding area when a non-PC creature (NPC or
// monster) moves from P(x1,y1) to Q(x2,y2).
//
// The three packets must be built before this method is called.
//////////////////////////////////////////////////////////////////////////////
void Zone::moveCreatureBroadcast(Creature* pCreature, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                                 bool bSendMove, bool bKnockback)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_BC_MOVE_CREATURE")

    try {
        Monster* pMonster = NULL;
        NPC* pNPC = NULL;

        Assert(pCreature != NULL);
        Assert(pCreature->isNPC() || pCreature->isMonster());

        // By the current policy GCMove carries the previous coordinate and the current direction.
        GCMove gcMove;
        if (bSendMove) {
            gcMove.setObjectID(pCreature->getObjectID());
            gcMove.setX(x1);
            gcMove.setY(y1);
            gcMove.setDir(pCreature->getDir());
        }
        GCKnockBack gcKnockback;
        if (bKnockback) {
            gcKnockback.setObjectID(pCreature->getObjectID());
            gcKnockback.setOrigin(x1, y1);
            gcKnockback.setTarget(x2, y2);
        }

#ifdef __USE_ENCRYPTER__
        SocketEncryptOutputStream outputStream(
            NULL, szPacketHeader + (bSendMove ? gcMove.getPacketSize() : gcKnockback.getPacketSize()) + 2);
        outputStream.setEncryptCode(m_EncryptCode);
#else
        SocketOutputStream outputStream(
            NULL, szPacketHeader + (bSendMove ? gcMove.getPacketSize() : gcKnockback.getPacketSize()) + 2);
#endif
        if (bSendMove)
            gcMove.writeHeaderNBody(outputStream);
        else
            gcKnockback.writeHeaderNBody(outputStream);

        // Build the GCAddNPC / GCAddMonster packet.
        Packet* pGCAddXXX = NULL;

        bool isMonster = !pCreature->isNPC();

        if (!isMonster) {
            pNPC = dynamic_cast<NPC*>(pCreature);
            GCAddNPC* pGCAddNPC = new GCAddNPC();
            makeGCAddNPC(pGCAddNPC, pNPC);
            pGCAddXXX = pGCAddNPC;
        } else // case of Monster
        {
            pMonster = dynamic_cast<Monster*>(pCreature);

            // Build the packet with the fully visible state (NULL).
            pGCAddXXX = createMonsterAddPacket(pMonster, NULL);

            // Remember the monster's state.
        }

        // Send GCDeleteObject when the creature leaves the vision area.
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pCreature->getObjectID());

        //////////////////////////////////////////////////////////////////////////////
        // Expand the vision area by 1 on all four sides.
        // The ON_SIGHT area grows with the facing direction.
        //////////////////////////////////////////////////////////////////////////////
        for (ZoneCoord_t ix = max(0, x2 - maxViewportWidth - 1), endx = min(m_Width - 1, x2 + maxViewportWidth + 1);
             ix <= endx; ix++) {
            for (ZoneCoord_t iy = max(0, y2 - maxViewportUpperHeight - 1),
                             endy = min(m_Height - 1, y2 + maxViewportLowerHeight + 1);
                 iy <= endy; iy++) {
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();
                forward_list<Object*>::const_iterator itr = objectList.begin();
                for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    Assert(*itr != NULL);

                    Creature* pPC = dynamic_cast<Creature*>(*itr);

                    Assert(pPC != NULL);

                    // GCMove and GCAddMonster go only to PCs; monsters do not need them.
                    if (pPC->isPC()) {
                        Assert(pPC->getPlayer() != NULL);

                        //////////////////////////////////////////////////////////////////////////////
                        // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                        // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                        //////////////////////////////////////////////////////////////////////////////
                        VisionState prevVS = pPC->getVisionState(x1, y1);
                        VisionState currVS = pPC->getVisionState(x2, y2);

                        // Moving from the invisible area straight into the inner vision
                        // area without passing the boundary is impossible.
                        // Only IN_SIGHT remains now.

                        // Fetch the ObservingEye effect.
                        //						//Assert( pEffectObservingEye != NULL );

                        if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                            if (isMonster) {
                                if (canSee(pPC, pMonster)) {
                                    pPC->getPlayer()->sendPacket(pGCAddXXX);
                                }
                            } else {
                                pPC->getPlayer()->sendPacket(pGCAddXXX);
                            }
                        } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                            pPC->getPlayer()->sendStream(&outputStream);
                        } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                            pPC->getPlayer()->sendPacket(&gcDeleteObject);
                        }

                        //--------------------------------------------------------------------------------
                        // After the broadcast, register this PC as a potential enemy.
                        //--------------------------------------------------------------------------------
                        if (pCreature->isMonster()) {
                            // pMonster was already dynamic_cast above.
                            VisionState vs = pMonster->getVisionState(ix, iy);
                            if (vs >= IN_SIGHT && pMonster->getAlignment() == ALIGNMENT_AGGRESSIVE) {
                                if (isPotentialEnemy(pMonster, pPC)) {
                                    pMonster->addPotentialEnemy(pPC);
                                }
                            }
                        }

                    } // if

                } // for

            } // for

        } // for

        // Delete the packet that was built.
        SAFE_DELETE(pGCAddXXX);

        // by sigi. 2002.12.15
    } catch (Throwable& t) {
        diagnosticFilelog("moveCreatureBroadcastError.log", "%s", t.toString().c_str());

        // Rethrow.
        throw;
    }

    __END_PROFILE_ZONE("Z_BC_MOVE_CREATURE")

    __END_CATCH
}
