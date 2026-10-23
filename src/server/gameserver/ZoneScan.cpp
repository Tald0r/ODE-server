//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneScan.cpp
// Description	: Zone scan and visibility: what a creature sees and who sees it.
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

//////////////////////////////////////////////////////////////////////////////
// When an effect such as detect invisibility goes away and something visible
// becomes invisible, GCDeleteObject is sent to pCreature so it drops the
// invisible creature it was watching, and the other way round as well.
//////////////////////////////////////////////////////////////////////////////
void Zone::updateInvisibleScan(Creature* pCreature) {
    Assert(pCreature != NULL && pCreature->isPC());

    Coord_t cx = pCreature->getX();
    Coord_t cy = pCreature->getY();
    Player* pPlayer = pCreature->getPlayer();

    // Fetch the Revealer effect.

    // Fetch the ObservingEye effect.
    EffectObservingEye* pEffectObservingEye = NULL;
    if (pCreature->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
        pEffectObservingEye =
            dynamic_cast<EffectObservingEye*>(pCreature->findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
    }

    EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
    // Fetch the GnomesWhisper effect.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
        pEffectGnomesWhisper =
            dynamic_cast<EffectGnomesWhisper*>(pCreature->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
    }

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            // Examine the darkness area.
            // pCreature is a slayer or an Ousters here (this is updateInvisibleScan).
            if (pCreature->isSlayer() || pCreature->isOusters()) {
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();
                forward_list<Object*>::const_iterator itr = objectList.begin();

                for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    if ((*itr)->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                        Creature* pPC = dynamic_cast<Creature*>(*itr);
                        Assert(pPC != NULL);

                        // Skip the creature itself.
                        if (pCreature == pPC || pPC->isFlag(Effect::EFFECT_CLASS_GHOST))
                            continue;

                        // For a hidden target,
                        // that is one in the SNIPING or INVISIBILITY state.
                        if (pPC->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) &&
                            pCreature->getVisionState(ix, iy) >= IN_SIGHT) {
                            // A creature with the Detect Invisibility effect, or a vampire, can see it,
                            // as can one whose ObservingEye effect is high enough for that target.
                            if (pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) || pCreature->isVampire() ||
                                (pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(pPC)) ||
                                (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeInvisibility())) {
                                if (pPC->isVampire()) {
                                    Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
                                    GCAddVampire gcAddVampire;

                                    makeGCAddVampire(&gcAddVampire, pVampire);
                                    pPlayer->sendPacket(&gcAddVampire);
                                } else if (pPC->isMonster()) {
                                    Monster* pMonster = dynamic_cast<Monster*>(pPC);

                                    // by sigi
                                    Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, pCreature);

                                    if (pAddMonsterPacket != NULL) {
                                        pPlayer->sendPacket(pAddMonsterPacket);

                                        delete pAddMonsterPacket;
                                    }
                                }
                            } else {
                                GCDeleteObject gcDO;
                                gcDO.setObjectID(pPC->getObjectID());
                                pPlayer->sendPacket(&gcDO);
                            }
                        } else if (pPC->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
                            if ((!pCreature->isVampire() &&
                                 pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY)))
                            //								|| ( pEffectRevealer != NULL &&
                            // pEffectRevealer->canSeeSniping( pPC ) ) )
                            {
                                if (pPC->isSlayer()) {
                                    Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

                                    GCAddSlayer gcAddSlayer;
                                    makeGCAddSlayer(&gcAddSlayer, pSlayer);
                                    pPlayer->sendPacket(&gcAddSlayer);
                                } else {
                                    throw Error("A vampire is in sniping mode.");
                                }
                            } else {
                                GCDeleteObject gcDO;
                                gcDO.setObjectID(pPC->getObjectID());
                                pPlayer->sendPacket(&gcDO);
                            }
                        }
                    }
                }
            } // darkness
        }
    }
}

//--------------------------------------------------------------------------------
// update hidden scan
// When an effect such as detect hidden goes away and something visible becomes
// hidden, GCDeleteObject is sent to pCreature so it drops the burrowing
// creature it was watching, and the other way round as well.
// ABCD
//--------------------------------------------------------------------------------
void Zone::updateHiddenScan(Creature* pCreature)

{
    Assert(pCreature != NULL && pCreature->isPC());

    Coord_t cx = pCreature->getX();
    Coord_t cy = pCreature->getY();
    Player* pPlayer = pCreature->getPlayer();

    // Fetch the Revealer effect.

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            // Examine the darkness area.
            // pCreature is of course a slayer here (this is updateHiddenScan).
            if (pCreature->isSlayer()) {
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

                for (forward_list<Object*>::const_iterator itr = objectList.begin();
                     itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    if ((*itr)->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                        Creature* pPC = dynamic_cast<Creature*>(*itr);
                        Assert(pPC != NULL);

                        // Skip the creature itself.
                        if (pCreature == pPC)
                            continue;

                        // For a hidden target.
                        if (pPC->isFlag(Effect::EFFECT_CLASS_HIDE) && pCreature->getVisionState(ix, iy) >= IN_SIGHT) {
                            if (pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) || pCreature->isVampire())
                            //								|| ( pEffectRevealer != NULL && pEffectRevealer->canSeeHide(
                            // pPC ) ) )
                            {
                                GCAddBurrowingCreature gcABC;
                                gcABC.setObjectID(pPC->getObjectID());
                                gcABC.setName(pPC->getName());
                                gcABC.setX(ix);
                                gcABC.setY(iy);
                                pPlayer->sendPacket(&gcABC);
                            } else {
                                GCDeleteObject gcDO;
                                gcDO.setObjectID(pPC->getObjectID());
                                pPlayer->sendPacket(&gcDO);
                            }
                        }
                    }
                }
            } // darkness
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// When the detect ability appears or goes away,
// add or remove creatures for the watching creature.
// For Ousters.
//////////////////////////////////////////////////////////////////////////////
void Zone::updateDetectScan(Creature* pCreature) {
    Assert(pCreature != NULL && pCreature->isPC());

    Coord_t cx = pCreature->getX();
    Coord_t cy = pCreature->getY();
    Player* pPlayer = pCreature->getPlayer();

    EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
    if (pCreature->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
        pEffectGnomesWhisper =
            dynamic_cast<EffectGnomesWhisper*>(pCreature->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
    }

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            if (pCreature->getVisionState(ix, iy) == OUT_OF_SIGHT)
                continue;

            if (pCreature->isOusters()) {
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();
                forward_list<Object*>::const_iterator itr = objectList.begin();

                for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                     itr++) {
                    if ((*itr)->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                        Creature* pPC = dynamic_cast<Creature*>(*itr);
                        Assert(pPC != NULL);

                        // Skip the creature itself.
                        if (pCreature == pPC || pPC->isFlag(Effect::EFFECT_CLASS_GHOST))
                            continue;

                        if (pPC->isSlayer()) {
                            if (pPC->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
                                if (canSee(pCreature, pPC, NULL, pEffectGnomesWhisper)) {
                                    Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

                                    GCAddSlayer gcAddSlayer;
                                    makeGCAddSlayer(&gcAddSlayer, pSlayer);
                                    pPlayer->sendPacket(&gcAddSlayer);
                                } else {
                                    GCDeleteObject gcDO;
                                    gcDO.setObjectID(pPC->getObjectID());
                                    pPlayer->sendPacket(&gcDO);
                                }
                            }
                        } else if (pPC->isVampire()) {
                            if (pPC->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
                                if (canSee(pCreature, pPC, NULL, pEffectGnomesWhisper)) {
                                    Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
                                    GCAddVampire gcAddVampire;

                                    makeGCAddVampire(&gcAddVampire, pVampire);
                                    pPlayer->sendPacket(&gcAddVampire);
                                } else {
                                    GCDeleteObject gcDO;
                                    gcDO.setObjectID(pPC->getObjectID());
                                    pPlayer->sendPacket(&gcDO);
                                }
                            }

                            if (pPC->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                                if (canSee(pCreature, pPC, NULL, pEffectGnomesWhisper)) {
                                    GCAddBurrowingCreature gcABC;
                                    gcABC.setObjectID(pPC->getObjectID());
                                    gcABC.setName(pPC->getName());
                                    gcABC.setX(ix);
                                    gcABC.setY(iy);
                                    pPlayer->sendPacket(&gcABC);
                                } else {
                                    GCDeleteObject gcDO;
                                    gcDO.setObjectID(pPC->getObjectID());
                                    pPlayer->sendPacket(&gcDO);
                                }
                            }
                        } else if (pPC->isMonster()) {
                            if (pPC->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) ||
                                pPC->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                                if (canSee(pCreature, pPC, NULL, pEffectGnomesWhisper)) {
                                    Monster* pMonster = dynamic_cast<Monster*>(pPC);

                                    Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, pCreature);

                                    if (pAddMonsterPacket != NULL) {
                                        pPlayer->sendPacket(pAddMonsterPacket);

                                        delete pAddMonsterPacket;
                                    }
                                } else {
                                    GCDeleteObject gcDO;
                                    gcDO.setObjectID(pPC->getObjectID());
                                    pPlayer->sendPacket(&gcDO);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------
// update mine scan
// When an effect such as detect mine goes away and a visible mine becomes
// invisible, GCDeleteObject is sent to pCreature.
//--------------------------------------------------------------------------------
void Zone::updateMineScan(Creature* pCreature) {
    Assert(pCreature != NULL && pCreature->isPC());

    Coord_t cx = pCreature->getX();
    Coord_t cy = pCreature->getY();
    Player* pPlayer = pCreature->getPlayer();

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            if (pCreature->getVisionState(ix, iy) == OUT_OF_SIGHT)
                continue;

            // pCreature is of course a slayer here (this is updateMineScan).
            if (pCreature->isSlayer()) {
                Item* pItem = m_pTiles[ix][iy].getItem();
                if (pItem) {
                    if (pItem->getItemClass() == Item::ITEM_CLASS_MINE && pItem->isFlag(Effect::EFFECT_CLASS_INSTALL)) {
                        if (pCreature->isFlag(Effect::EFFECT_CLASS_REVEALER)) {
                            GCAddInstalledMineToZone gcAddMine;
                            gcAddMine.setObjectID(pItem->getObjectID());
                            gcAddMine.setX(ix);
                            gcAddMine.setY(iy);
                            gcAddMine.setItemClass(pItem->getItemClass());
                            gcAddMine.setItemType(pItem->getItemType());
                            gcAddMine.setOptionType(pItem->getOptionTypeList());
                            gcAddMine.setDurability(pItem->getDurability());

                            pPlayer->sendPacket(&gcAddMine);
                        } else {
                            GCDeleteObject gcDO;
                            gcDO.setObjectID(pItem->getObjectID());
                            pPlayer->sendPacket(&gcDO);
                        }
                    }
                }
            } // darkness
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// scan
// Collect information on every object within sight of (x,y).
//////////////////////////////////////////////////////////////////////////////
void Zone::scan(Creature* pPC, ZoneCoord_t cx, ZoneCoord_t cy, Packet* pPacket) {
    __BEGIN_PROFILE_ZONE("Z_SCAN")

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + pPacket->getPacketSize() + 2);
#endif
    pPacket->writeHeaderNBody(outputStream);

    Assert(pPC->isPC());

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer);

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            if (pPC->getVisionState(ix, iy) == OUT_OF_SIGHT)
                continue;

            const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

            for (forward_list<Object*>::const_iterator itr = objectList.begin(); itr != objectList.end(); itr++) {
                Assert(*itr != NULL);

                //--------------------------------------------------------------------------------
                //
                // Build the GCAddXXX packet that fits each object's OBJECT CLASS and send it
                // to the owner.
                //
                // *NOTES*
                //
                // The object CLASS that occurs most often should come first among the cases.
                //
                //--------------------------------------------------------------------------------
                switch ((*itr)->getObjectClass()) {
                //--------------------------------------------------------------------------------
                // A creature on the tile.
                //--------------------------------------------------------------------------------
                case Object::OBJECT_CLASS_CREATURE: {
                    //--------------------------------------------------------------------------------
                    // For a PC, pPacket must be sent; for a non-PC it need not be.
                    // Information on every creature is sent to the owner either way.
                    //--------------------------------------------------------------------------------
                    Creature* pCreature = dynamic_cast<Creature*>(*itr);
                    Assert(pCreature != NULL);

                    if (pCreature == pPC) // no need to receive one's own information
                        continue;

                    // Skip what cannot be seen.
                    bool bCanSee = canSee(pPC, pCreature);

                    switch (pCreature->getCreatureClass()) {
                    case Creature::CREATURE_CLASS_MONSTER: {
                        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                        if (bCanSee) {
                            // by sigi
                            Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, pPC);

                            if (pAddMonsterPacket != NULL) {
                                pPlayer->sendPacket(pAddMonsterPacket);

                                delete pAddMonsterPacket;
                            }
                        }

                        //--------------------------------------------------------------------------------
                        // If the monster can see the PC, make the PC the monster's enemy.
                        //--------------------------------------------------------------------------------
                        VisionState vs = pMonster->getVisionState(cx, cy);

                        // Only an aggressive monster registers an enemy.
                        if (vs >= IN_SIGHT && pMonster->getAlignment() == ALIGNMENT_AGGRESSIVE) {
                            if (isPotentialEnemy(pMonster, pPC)) {
                                pMonster->addPotentialEnemy(pPC);
                            }
                        }
                    } break;

                    case Creature::CREATURE_CLASS_SLAYER: {
                        // If this creature can see that square (with respect to darkness).
                        if (bCanSee) {
                            //											if
                            //(!pCreature->isFlag(Effect::EFFECT_CLASS_GHOST)
                            //												&&
                            //! pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
                            // pPC->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) )
                            //													|| ( pEffectRevealer != NULL &&
                            // pEffectRevealer->canSeeSniping( pCreature ) ) )
                            //											{
                            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                            GCAddSlayer gcAddSlayer;
                            makeGCAddSlayer(&gcAddSlayer, pSlayer);
                            pPlayer->sendPacket(&gcAddSlayer);
                            //											}
                        }

                        // If the other side (a slayer) can see this creature.
                        if (pPacket && pCreature->getVisionState(cx, cy) >= IN_SIGHT) {
                            Assert(pCreature->getPlayer() != NULL);
                            // Handled by canSee.
                            if (canSee(pCreature, pPC)) {
                                pCreature->getPlayer()->sendStream(&outputStream);
                            }
                        }
                    } break;

                    case Creature::CREATURE_CLASS_VAMPIRE: {
                        if (bCanSee) {
                            // Fetch the ObservingEye effect if the PC has one.
                            //												//Assert( pEffectObservingEye != NULL );

                            if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                                GCAddBurrowingCreature gcABC;
                                gcABC.setObjectID(pCreature->getObjectID());
                                gcABC.setName(pCreature->getName());
                                gcABC.setX(ix);
                                gcABC.setY(iy);

                                pPlayer->sendPacket(&gcABC);
                                //				}
                            } else {
                                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                                GCAddVampire gcAddVampire;
                                makeGCAddVampire(&gcAddVampire, pVampire);
                                pPlayer->sendPacket(&gcAddVampire);
                                //				}
                            }
                        }

                        // If the other side can see this creature.
                        // The other side is a vampire, so as long as it is in sight darkness does not matter.
                        // Against a vampire, sniping mode makes the creature completely invisible.
                        //
                        // But the way scan works, sniping mode cannot be skipped without clearing it.
                        // Handled by canSee.
                        if (pPacket && pCreature->getVisionState(cx, cy) >= IN_SIGHT && canSee(pCreature, pPC)) {
                            Assert(pCreature->getPlayer() != NULL);
                            pCreature->getPlayer()->sendStream(&outputStream);
                        }
                    } break;

                    case Creature::CREATURE_CLASS_OUSTERS: {
                        if (bCanSee) {
                            // Fetch the ObservingEye effect if the PC has one.
                            //												//Assert( pEffectObservingEye != NULL );

                            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                            GCAddOusters gcAddOusters;
                            makeGCAddOusters(&gcAddOusters, pOusters);
                            pPlayer->sendPacket(&gcAddOusters);
                            //											}
                        }

                        if (pPacket && pCreature->getVisionState(cx, cy) >= IN_SIGHT && canSee(pCreature, pPC)) {
                            Assert(pCreature->getPlayer() != NULL);
                            pCreature->getPlayer()->sendStream(&outputStream);
                        }
                    } break;

                    case Creature::CREATURE_CLASS_NPC: {
                        if (bCanSee) {
                            NPC* pNPC = dynamic_cast<NPC*>(pCreature);
                            GCAddNPC gcAddNPC;
                            makeGCAddNPC(&gcAddNPC, pNPC);
                            pPlayer->sendPacket(&gcAddNPC);
                        }
                    } break;

                    default:
                        throw Error("invalid creature class");

                    } // switch (pCreature->getCreatureClass())
                } // case Object::OBJECT_CLASS_CREATURE :

                break;

                //--------------------------------------------------------------------------------
                // An item on the tile.
                //--------------------------------------------------------------------------------
                case Object::OBJECT_CLASS_ITEM: {
                    Item* pItem = dynamic_cast<Item*>(*itr);

                    if (pItem->getItemClass() == Item::ITEM_CLASS_CORPSE) {
                        switch (pItem->getItemType()) {
                        case SLAYER_CORPSE: {
                            SlayerCorpse* pSlayerCorpse = dynamic_cast<SlayerCorpse*>(pItem);
                            GCAddSlayerCorpse gcAddSlayerCorpse;
                            makeGCAddSlayerCorpse(&gcAddSlayerCorpse, pSlayerCorpse);
                            pPlayer->sendPacket(&gcAddSlayerCorpse);
                        } break;
                        case VAMPIRE_CORPSE: {
                            VampireCorpse* pVampireCorpse = dynamic_cast<VampireCorpse*>(pItem);
                            GCAddVampireCorpse gcAddVampireCorpse;
                            makeGCAddVampireCorpse(&gcAddVampireCorpse, pVampireCorpse);
                            pPlayer->sendPacket(&gcAddVampireCorpse);
                        } break;
                        case OUSTERS_CORPSE: {
                            OustersCorpse* pOustersCorpse = dynamic_cast<OustersCorpse*>(pItem);
                            GCAddOustersCorpse gcAddOustersCorpse;
                            makeGCAddOustersCorpse(&gcAddOustersCorpse, pOustersCorpse);
                            pPlayer->sendPacket(&gcAddOustersCorpse);
                        } break;
                        case NPC_CORPSE: {
                            throw UnsupportedError();
                        } break;
                        case MONSTER_CORPSE: {
                            MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                            GCAddMonsterCorpse gcAddMonsterCorpse;
                            makeGCAddMonsterCorpse(&gcAddMonsterCorpse, pMonsterCorpse, ix, iy);
                            pPlayer->sendPacket(&gcAddMonsterCorpse);

                            sendRelicEffect(pMonsterCorpse, pPlayer);
                        } break;
                        } // switch
                    } else if (pItem->getItemClass() == Item::ITEM_CLASS_MINE &&
                               pItem->isFlag(Effect::EFFECT_CLASS_INSTALL)) {
                        if (pPC->isFlag(Effect::EFFECT_CLASS_REVEALER)) {
                            GCAddInstalledMineToZone gcAddMine;
                            gcAddMine.setObjectID(pItem->getObjectID());
                            gcAddMine.setX(cx);
                            gcAddMine.setY(cy);
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
                    //							}
                } break;

                //--------------------------------------------------------------------------------
                // An effect on the tile.
                //--------------------------------------------------------------------------------
                case Object::OBJECT_CLASS_EFFECT: {
                    Effect* pEffect = dynamic_cast<Effect*>(*itr);
                    if (pEffect->getEffectClass() == Effect::EFFECT_CLASS_VAMPIRE_PORTAL) {
                        EffectVampirePortal* pEffectVampirePortal = dynamic_cast<EffectVampirePortal*>(pEffect);
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

                        // A sanctuary sends the packet only for its center coordinate.
                        if (centerX == ix && centerY == iy) {
                            GCAddEffectToTile gcAddEffectToTile;

                            gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                            gcAddEffectToTile.setXY(ix, iy);
                            gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                            gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                            pPlayer->sendPacket(&gcAddEffectToTile);
                        }
                    }
                    // Check whether this is a broadcasting effect.
                    else if (pEffect->isBroadcastingEffect()) {
                        GCAddEffectToTile gcAddEffectToTile;

                        gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                        gcAddEffectToTile.setXY(ix, iy);
                        gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                        gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                        pPlayer->sendPacket(&gcAddEffectToTile);
                    }
                    //							}
                } break;


                //--------------------------------------------------------------------------------
                // An obstacle on the tile.
                //--------------------------------------------------------------------------------
                case Object::OBJECT_CLASS_OBSTACLE: {
                    /*
                     */
                } break;

                //--------------------------------------------------------------------------------
                // A portal on the tile.
                //--------------------------------------------------------------------------------
                case Object::OBJECT_CLASS_PORTAL: {
                    /*
                     */
                } break;

                default:
                    throw Error("invalid object class");

                } // switch ((*itr)->getObjectClass())
            } // for

        } // for
    } // for

    __END_PROFILE_ZONE("Z_SCAN")
}

//////////////////////////////////////////////////////////////////////////////
// When a monster respawns at (x,y), send a GCAddXXX packet to every PC within
// sight and treat those PCs as potential enemies.
//////////////////////////////////////////////////////////////////////////////
void Zone::scanPC(Creature* pCreature) {
    __BEGIN_PROFILE_ZONE("Z_SCAN_PC")

    Monster* pMonster = NULL;

    Assert(pCreature != NULL);

    ZoneCoord_t cx = pCreature->getX();
    ZoneCoord_t cy = pCreature->getY();

    Packet* pGCAddXXX = NULL;

    // Build the packet according to the creature's class.
    Creature::CreatureClass CClass = pCreature->getCreatureClass();

    bool isMonster = pCreature->isMonster();

    if (CClass == Creature::CREATURE_CLASS_MONSTER) {
        pMonster = dynamic_cast<Monster*>(pCreature);

        // by sigi
        pGCAddXXX = createMonsterAddPacket(pMonster, NULL);


    } else if (CClass == Creature::CREATURE_CLASS_NPC) {
        NPC* pNPC = dynamic_cast<NPC*>(pCreature);

        GCAddNPC* pGCAddNPC = new GCAddNPC;

        makeGCAddNPC(pGCAddNPC, pNPC);

        pGCAddXXX = pGCAddNPC;
    } else {
        Assert(false);
    }

    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1), endx = min(m_Width - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();
            forward_list<Object*>::const_iterator itr = objectList.begin();

            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                if ((*itr)->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    Creature* pPC = dynamic_cast<Creature*>(*itr);
                    Assert(pPC != NULL);

                    // A PC that can see the creature.
                    if (pPC->isPC() && pPC->getVisionState(cx, cy) >= IN_SIGHT && canSee(pPC, pCreature))
                    //						&& !pPC->isFlag(Effect::EFFECT_CLASS_GHOST)
                    {
                        // Fetch the Revealer effect if the creature has one.

                        // Fetch the ObservingEye effect if the creature has one.
                        //							//Assert( pEffectObservingEye != NULL );

                        // A monster never snipes, so only DETECT_HIDDEN and INVISIBILITY are checked.
                        pPC->getPlayer()->sendPacket(pGCAddXXX);
                        //						}

                        if (isMonster) {
                            // Can the monster at (cx,cy) see the PC at (ix,iy)?
                            VisionState vs = pMonster->getVisionState(ix, iy);
                            if (vs >= IN_SIGHT && pMonster->getAlignment() == ALIGNMENT_AGGRESSIVE &&
                                canSee(pCreature, pPC)) {
                                if (isPotentialEnemy(pMonster, pPC)) {
                                    pMonster->addPotentialEnemy(pPC);
                                }
                            }
                        }
                    }

                } // if (creature)

            } // for itr

        } // for y
    } // for x

    if (pGCAddXXX != NULL) {
        delete pGCAddXXX;
    }

    __END_PROFILE_ZONE("Z_SCAN_PC")
}

//////////////////////////////////////////////////////////////////////////////
// Return the list of players that can see pTargetCreature.
// **********************************
//////////////////////////////////////////////////////////////////////////////
list<Creature*> Zone::getWatcherList(ZoneCoord_t x, ZoneCoord_t y, Creature* pTargetCreature)

{
    list<Creature*> cList;

    __BEGIN_PROFILE_ZONE("Z_GET_WATCHERLIST")

    if (pTargetCreature == NULL)
        return cList;

    ////////////////////////////////////////////////////////////
    // Grow the sight area by one in every direction, because the ON_SIGHT area
    // grows with the facing direction.
    ////////////////////////////////////////////////////////////
    for (ZoneCoord_t ix = max(0, x - maxViewportWidth - 1), endx = min(m_Width - 1, x + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, y - maxViewportUpperHeight - 1),
                         endy = min(m_Height - 1, y + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();
            forward_list<Object*>::const_iterator itr = objectList.begin();
            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                Assert(*itr != NULL);

                Creature* pCreature = dynamic_cast<Creature*>(*itr);

                Assert(pCreature != NULL);

                if (pCreature->isPC()) {
                    Assert(pCreature->getPlayer() != NULL);

                    // No need to receive one's own information.
                    if (pTargetCreature == pCreature || pCreature->isFlag(Effect::EFFECT_CLASS_GHOST))
                        continue;

                    VisionState vs = pCreature->getVisionState(x, y);

                    if (vs >= IN_SIGHT) {
                        if (canSee(pCreature, pTargetCreature)) {
                            cList.push_back(pCreature);
                        }
                    }
                } // if

            } // for

        } // for

    } // for

    __END_PROFILE_ZONE("Z_GET_WATCHERLIST")

    return cList;
}

void Zone::monsterScan(Monster* pMonster, ZoneCoord_t x, ZoneCoord_t y, Dir_t dir)

{
    // [TEST CODE]

    switch (pMonster->getMonsterType()) {
    case 717:
    case 721:
    case 723:
    case 724:
    case 725: {
        unordered_map<ObjectID_t, Creature*>::iterator itr = m_pPCManager->getCreatures().begin();
        unordered_map<ObjectID_t, Creature*>::iterator endItr = m_pPCManager->getCreatures().end();

        for (; itr != endItr; ++itr) {
            pMonster->addPotentialEnemy(itr->second);
        }
        break;
    }
    default:
        break;
    }

    ZoneCoord_t x2 = x;
    ZoneCoord_t y2 = y;

    //////////////////////////////////////////////////////////////////////////////
    // Grow the sight area by one in every direction, because the ON_SIGHT area
    // grows with the facing direction.
    //////////////////////////////////////////////////////////////////////////////
    int sight = pMonster->getSight();

    for (ZoneCoord_t ix = max(0, x2 - sight - 1), endx = min(m_Width - 1, x2 + sight + 1); ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, y2 - sight - 1), endy = min(m_Height - 1, y2 + sight + 1); iy <= endy; iy++) {
            // Iterate over every object on the current tile.
            const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();

            //
            // An if combined with a do-while is used here so that
            // pVisionInfo->getVisionState() is checked only when
            // there is actually an object on the tile.
            //
            if (itr != objectList.end()) {
                do {
                    Assert(*itr != NULL);

                    Object::ObjectClass OClass = (*itr)->getObjectClass();

                    ////////////////////////////////////////////////////////////
                    // Build the GCAddXXX packet that fits each object's OBJECT CLASS
                    // and send it to the owner.
                    ////////////////////////////////////////////////////////////

                    ////////////////////////////////////////////////////////////
                    // A creature on the tile.
                    ////////////////////////////////////////////////////////////
                    if (OClass == Object::OBJECT_CLASS_CREATURE) {
                        Creature* pCreature = dynamic_cast<Creature*>(*itr);
                        Assert(pCreature != NULL);

                        Creature::CreatureClass CClass = pCreature->getCreatureClass();

                        if (pCreature->isPC()) {
                            if (pMonster->isEnemyToAttack(pCreature)) {
                                pMonster->addPotentialEnemy(pCreature);
                            }
                        } else if (CClass == Creature::CREATURE_CLASS_MONSTER) {
                            Monster* pOtherMonster = dynamic_cast<Monster*>(pCreature);

                            VisionState vs = pOtherMonster->getVisionState(x2, y2);

                            // Register the two as potential enemies when in sight.
                            if (vs >= IN_SIGHT) {
                                if (isPotentialEnemy(pMonster, pOtherMonster)) {
                                    pMonster->addPotentialEnemy(pOtherMonster);
                                    pOtherMonster->addPotentialEnemy(pMonster);
                                }
                            }
                        }
                    } // if (OClass==OBJECT_CLASS_CREATURE)
                } while (++itr != objectList.end()); // do
            } // if (itr != objectList.end())
        } // for iy
    } // for ix
}
