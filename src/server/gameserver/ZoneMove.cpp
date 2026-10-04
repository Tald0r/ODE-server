//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneMove.cpp
// Description	: Zone movement: stepping a creature to a neighbouring tile and fast moves.
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
#include "DynamicZone.h"
#include "EffectAddItem.h"
#include "EffectAddItemToCorpse.h"
#include "EffectCastingTrap.h"
#include "EffectContinualGroundAttack.h"
#include "EffectDarkness.h"
#include "EffectDecayCorpse.h"
#include "EffectDecayItem.h"
#include "EffectDeleteItem.h"
#include "EffectGnomesWhisper.h"
#include "EffectHasBloodBible.h"
#include "EffectHasCastleSymbol.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectLoaderManager.h"
#include "EffectManager.h"
#include "EffectObservingEye.h"
#include "EffectPKZoneRegen.h"
#include "EffectRelicTable.h"
#include "EffectSanctuary.h"
#include "EffectSchedule.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EffectSlayerRelic.h"
#include "EffectTransportItem.h"
#include "EffectTransportItemToCorpse.h"
#include "EffectVampirePortal.h"
#include "EffectVampireRelic.h"
#include "EventTransport.h"
#include "FlagSet.h"
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
#include "GDRLairManager.h"
#include "GGCommand.h"
#include "GQuestManager.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "LoginServerManager.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "NPCManager.h"
#include "NicknameBook.h"
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
#include "Profile.h"
#include "Properties.h"
#include "QuestManager.h"
#include "RegenZoneManager.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "ShrineInfoManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "Store.h"
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
#include "ZoneInternal.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "item/Motorcycle.h"
#include "item/VampirePortalItem.h"
#include "repository/ComebackEventRepository.h"
#include "repository/MessageRepository.h"
#include "repository/ZoneInfoRepository.h"


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

int g_FastMoveSearchX[8][4] = {
    {0, 1, 1, 1},    // LEFT
    {0, 1, 0, 1},    // LEFTDOWN
    {0, 0, -1, 1},   // DOWN
    {0, -1, 0, -1},  // RIGHTDOWN
    {0, -1, -1, -1}, // RIGHT
    {0, -1, 0, -1},  // RIGHTUP
    {0, 0, -1, 1},   // UP
    {0, 1, 0, 1},    // LEFTUP
};

int g_FastMoveSearchY[8][4] = {
    {0, 0, -1, 1},   // LEFT
    {0, -1, -1, 0},  // LEFTDOWN
    {0, -1, -1, -1}, // DOWN
    {0, -1, -1, 0},  // RIGHTDOWN
    {0, 0, -1, 1},   // RIGHT
    {0, 1, 1, 0},    // RIGHTUP
    {0, 1, 1, 1},    // UP
    {0, 1, 1, 0},    // LEFTUP
};

//////////////////////////////////////////////////////////////////////////////
// Zone processing generally does not use a mutex,
// because it is driven only by the ZoneGroupThread. Adding a new PC to a
// zone, however, happens in the IPM, which is why a mutex member is needed
// and the method below has to be locked.
//////////////////////////////////////////////////////////////////////////////
void Zone::pushPC(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_PCListQueue.push_back(pCreature);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Move the PC at P(cx,cy) in direction dir to Q(nx,ny).
// Then broadcast the move information to the surrounding PCs.
//
// *CAUTION*
// Moving a non-PC creature (NPC, Monster) uses moveCreature.
//////////////////////////////////////////////////////////////////////////////
void Zone::movePC(Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir)

{
    __BEGIN_TRY

    Assert(pCreature->isPC());

    Player* pPlayer = pCreature->getPlayer();
    Assert(pPlayer != NULL);

    if (dir >= DIR_MAX || !isAbleToMove(pCreature)) {
        GCMoveError gcMoveError(pCreature->getX(), pCreature->getY());
        pPlayer->sendPacket(&gcMoveError);
        filelog("ZoneDebug.txt", "movePC - 1\n\r");
        return;
    }

    const int threshold = 6;
    ////////////////////////////////////////////////////////////
    // First check whether the creature is trying to jump.
    // If it jumped slightly, send a GCMoveError packet.
    //
    // OX, OY : the player's current coordinates
    // CX, CY : the target coordinates
    //
    // max(0, OX - threshold) <= CX <= min(OX + threshold, ZONEWIDTH-1)
    // max(0, OY - threshold) <= CY <= min(OY + threshold, ZONEHEIGHT-1)
    // must hold for the move to be valid.
    ////////////////////////////////////////////////////////////
    if (pCreature->getX() != cx || pCreature->getY() != cy) {
        if (cx >= max(0, pCreature->getX() - threshold) && cx <= min(m_Width - 1, pCreature->getX() + threshold) &&
            cy >= max(0, pCreature->getY() - threshold) && cy <= min(m_Height - 1, pCreature->getY() + threshold)) {
            // A jump within the allowed error range is simply ignored.

            filelog("ZoneDebug.txt", "movePC - 2\n\r");
            return;
        } else {
            // A jump beyond the allowed error range blocks the connection.

            GCMoveError gcMoveError(cx, cy);
            pPlayer->sendPacket(&gcMoveError);
            filelog("ZoneDebug.txt", "movePC - 3\n\r");
            return;
        }
    }

    // Compute the next coordinates.
    int nx = cx;
    int ny = cy;

    //////////////////////////////////////////////////////////////////////////////
    // *CAUTION*
    // A packet moving from the border to outside the border must not arrive.
    // e.g. a LEFT move from (0,10) cannot arrive. The same goes for an UP move from (10,0).
    //////////////////////////////////////////////////////////////////////////////
    nx = nx + dirMoveMask[dir].x;
    ny = ny + dirMoveMask[dir].y;

    VSRect rect(0, 0, m_Width - 1, m_Height - 1);
    if (!rect.ptInRect(nx, ny))
        throw InvalidProtocolException("invalid coordination");

    ////////////////////////////////////////////////////////////
    // If the destination is blocked, send GCMoveError.
    // (The position the PC stands on has to be blocked too.)
    ////////////////////////////////////////////////////////////
    Tile& newTile = m_pTiles[nx][ny];
    Tile& oldTile = m_pTiles[cx][cy];

    // While carrying a relic, safe zones cannot be entered.
    if (pCreature->hasRelicItem() || pCreature->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
        ZoneLevel_t ZoneLevel = getZoneLevel(nx, ny);

        // A Slayer cannot enter a Slayer safe zone.
        // A Vampire cannot enter a Vampire safe zone.
        // A common safe zone cannot be entered.
        if ((pCreature->isSlayer() && (ZoneLevel & SLAYER_SAFE_ZONE)) ||
            (pCreature->isVampire() && (ZoneLevel & VAMPIRE_SAFE_ZONE)) ||
            (pCreature->isOusters() && (ZoneLevel & OUSTERS_SAFE_ZONE)) || (ZoneLevel & COMPLETE_SAFE_ZONE)) {
            GCMoveError gcMoveError(cx, cy);
            pPlayer->sendPacket(&gcMoveError);
            filelog("ZoneDebug.txt", "movePC - 4\n\r");
            return;
        }
    }

    if (newTile.hasCreature(pCreature->getMoveMode())) {
        Creature* pTargetCreature = newTile.getCreature(pCreature->getMoveMode());
        if (pTargetCreature != NULL && pTargetCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
            if (pMonster->getMonsterType() >= 738 && pMonster->getMonsterType() <= 740) {
                pMonster->addEnemy(pTargetCreature);
                pMonster->setHP(0);
            }
        }

        GCMoveError gcMoveError(cx, cy);
        pPlayer->sendPacket(&gcMoveError);
    }

    if (newTile.isBlocked(pCreature->getMoveMode())
        // If BloodyWallBlocked or
        // Sanctuary is in effect, the move is not allowed.
        || (newTile.hasEffect() && (newTile.getEffect(Effect::EFFECT_CLASS_BLOODY_WALL_BLOCKED) ||
                                    newTile.getEffect(Effect::EFFECT_CLASS_SANCTUARY))) ||
        oldTile.getEffect(Effect::EFFECT_CLASS_SANCTUARY) != NULL) {
        GCMoveError gcMoveError(cx, cy);
        pPlayer->sendPacket(&gcMoveError);
    } else {
        // First change the creature's coordinates.
        pCreature->setXYDir(nx, ny, dir);

        try {
            // Delete the creature from the previous tile.
            m_pTiles[cx][cy].deleteCreature(pCreature->getObjectID());

            // Add the creature to the new tile.
            if (!newTile.addCreature(pCreature)) {
                // The portal was activated.
                return;
            }

            try {
                checkMine(this, pCreature, nx, ny);
                checkTrap(this, pCreature);
            } catch (Throwable& t) {
                diagnosticFilelog("CheckMineBug.txt", "%s : %s", "movePC", t.toString().c_str());
            }

            // When GCMoveOK is sent to the client, (nx,ny) must be the destination and
            // dir the facing (movement) direction. That is the current policy.
            GCMoveOK gcMoveOK(nx, ny, dir);
            pPlayer->sendPacket(&gcMoveOK);

            // Broadcast the GCMove/GCAddSlayer/GCAddVampire packets automatically.
            movePCBroadcast(pCreature, cx, cy, nx, ny);
        } catch (NoSuchElementException& nsee) {
            throw Error("The creature is not on the previous tile.");
        } catch (DuplicatedException& de) {
            throw Error("A creature is already on the new tile.");
        } catch (PortalException&) {
        } catch (Error& e) {
            filelog("assertTile.txt", "Zone::movePC : %s", e.toString().c_str());
            throw;
        }
    }
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// This method is used to move a non-PC creature (NPC, Monster).
//
// *CAUTION*
//
// Here (nx,ny,dir) is the next coordinate the creature reaches and its facing direction.
// It must already have been verified that this coordinate is empty. (path finding routine)
//////////////////////////////////////////////////////////////////////////////
void Zone::moveCreature(Creature* pCreature, ZoneCoord_t nx, ZoneCoord_t ny, Dir_t dir)

{
    __BEGIN_TRY

    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    ZoneCoord_t cx = pCreature->getX();
    ZoneCoord_t cy = pCreature->getY();

    // Delete the creature from the previous tile and add it to the next tile.
    try {
        // Delete the creature from the previous tile.
        m_pTiles[cx][cy].deleteCreature(pCreature->getObjectID());

        // Add the creature to the destination tile.
        m_pTiles[nx][ny].addCreature(pCreature);

        // Set the creature's coordinates and direction.
        pCreature->setXYDir(nx, ny, dir);

        try {
            checkMine(this, pCreature, nx, ny);
            checkTrap(this, pCreature);
        } catch (Throwable& t) {
            diagnosticFilelog("CheckMineBug.txt", "%s : %s", "moveCreature", t.toString().c_str());
        }

    } catch (NoSuchElementException& nsee) {
        throw Error("The creature is not on the previous tile.");
    } catch (DuplicatedException& de) {
        throw Error("A creature is already on the new tile.");
    } catch (Error& e) {
        filelog("assertTile.txt", "Zone::moveCreature : %s", e.toString().c_str());
        throw;
    }

    // Broadcast GCMove and GCAddMonster/GCAddNPC automatically.
    moveCreatureBroadcast(pCreature, cx, cy, nx, ny);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Broadcasts a creature's fast move from P(x1,y1) to Q(x2,y2) to the PCs
// in the surrounding area.
// for Skill FlashSliding, ShadowWalk
//////////////////////////////////////////////////////////////////////////////
bool Zone::moveFastPC(Creature* pPC, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                      SkillType_t skillType) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    // This method targets a PC.
    Assert(pPC->isPC());

    if (!isAbleToMove(pPC))
        return false;


    // While carrying a relic, safe zones cannot be entered.
    if (pPC->hasRelicItem()) {
        return false;
    }

    if (pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pPC->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))
        return false;

    if (m_ZoneID == 1410 || m_ZoneID == 1411)
        return false;

    Tile& rTile = getTile(x1, y1);

    if (rTile.getEffect(Effect::EFFECT_CLASS_ON_BRIDGE) != NULL)
        return false;

    // Find a suitable destination point.
    // Four candidate points in front are searched.
    Dir_t dir = calcDirection(x1, y1, x2, y2);

    // g_FastMoveSearchX, Y are used for the search.
    int* searchX = g_FastMoveSearchX[dir];
    int* searchY = g_FastMoveSearchY[dir];

    // Check whether the tile is empty.
    int i = 0;
    for (i = 0; i < 4; i++) {
        int targetX = x2 + searchX[i], targetY = y2 + searchY[i];
        if (targetX >= 0 && targetX < m_Width && targetY >= 0 && targetY < m_Height &&
            !m_pTiles[targetX][targetY].isBlocked(pPC->getMoveMode()) && !m_pTiles[targetX][targetY].hasPortal() &&
            // Sanctuary must not be in effect.
            m_pTiles[targetX][targetY].getEffect(Effect::EFFECT_CLASS_SANCTUARY) == NULL &&
            m_pTiles[x1][y1].getEffect(Effect::EFFECT_CLASS_SANCTUARY) == NULL) {
            x2 = targetX;
            y2 = targetY;
            break;
        }
    }
    if (i == 4) {
        return false; // No empty tile was found!
    }

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer);

    GCFastMove gcFastMove;
    gcFastMove.setObjectID(pPC->getObjectID());
    gcFastMove.setXY(x1, y1, x2, y2);
    gcFastMove.setSkillType(skillType);

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + gcFastMove.getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + gcFastMove.getPacketSize() + 2);
#endif
    gcFastMove.writeHeaderNBody(outputStream);

    pPlayer->sendStream(&outputStream);

    // Quest.
    dynamic_cast<PlayerCreature*>(pPC)->getGQuestManager()->fastMove();

    //////////////////////////////////////////////////////////////
    // The kind of move....
    // Depending on it, GCDelete or Add may have to be sent.

    // Change the PC's coordinates.
    pPC->setXYDir(x2, y2, dir);
    // Delete the creature from the previous tile.

    try {
        m_pTiles[x1][y1].deleteCreature(pPC->getObjectID());
    } catch (Error& e) {
        filelog("assertTile.txt", "moveFastPC : %s", e.toString().c_str());
        throw;
    }

    // Add the creature to the new tile.
    m_pTiles[x2][y2].addCreature(pPC);

    try {
        checkMine(this, pPC, x2, y2);
        checkTrap(this, pPC);
    } catch (Throwable& t) {
        diagnosticFilelog("CheckMineBug.txt", "%s : %s", "moveFastPC", t.toString().c_str());
    }

    if (pPC->isFlag(Effect::EFFECT_CLASS_GHOST)) {
    }


    //--------------------------------------------------------------------------------
    // Prepare the GCAddSlayer/GCAddVampire packet.
    // By the current policy, the GCAdd packet is based on the current coordinates.
    //--------------------------------------------------------------------------------
    Packet* pGCAddXXX = NULL;

    if (pPC->getCreatureClass() == Creature::CREATURE_CLASS_SLAYER) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        GCAddSlayer* pGCAddSlayer = new GCAddSlayer;
        makeGCAddSlayer(pGCAddSlayer, pSlayer);

        pGCAddXXX = pGCAddSlayer;
    } else if (pPC->getCreatureClass() == Creature::CREATURE_CLASS_VAMPIRE) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

        // Moving while hidden is not possible,
        // but prepare for the future.
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

    //--------------------------------------------------------------------------------
    // Prepare the GCDeleteObject packet.
    //--------------------------------------------------------------------------------
    GCDeleteObject gcDeleteObject;
    gcDeleteObject.setObjectID(pPC->getObjectID());


    // Compute the total range of sight.
    ZoneCoord_t minX, maxX, minY, maxY;
    if (x1 < x2) {
        minX = max(0, x1 - maxViewportWidth);
        maxX = min(m_Width - 1, x2 + maxViewportWidth);
    } else {
        minX = max(0, x2 - maxViewportWidth);
        maxX = min(m_Width - 1, x1 + maxViewportWidth);
    }
    if (y1 < y2) {
        minY = max(0, y1 - maxViewportUpperHeight);
        maxY = min(m_Height - 1, y2 + maxViewportLowerHeight);
    } else {
        minY = max(0, y2 - maxViewportUpperHeight);
        maxY = min(m_Height - 1, y1 + maxViewportLowerHeight);
    }


    // Get the ObservingEye effect.
    //		//Assert( pEffectObservingEye != NULL );
    //
    for (ZoneCoord_t ix = minX; ix <= maxX; ix++) {
        for (ZoneCoord_t iy = minY; iy <= maxY; iy++) {
            const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();

            // Because of visionInfo,
            // the structure is an if plus a do-while rather than a plain if.
            if (itr != objectList.end()) {
                // How is I(ix,iy) seen from the previous coordinate P(x1,y1)?
                VisionState prevVisionState = VisionInfoManager::getVisionState(x1, y1, ix, iy);
                // How is I(ix,iy) seen from the current coordinate Q(x2,y2)?
                VisionState curVisionState = VisionInfoManager::getVisionState(x2, y2, ix, iy);

                do {
                    Assert(*itr != NULL);

                    //--------------------------------------------------------------------------------
                    //
                    // Build the GCAddXXX packet matching each object's OBJECT CLASS and
                    // send it to the owner.
                    //
                    // *NOTES*
                    //
                    // The object CLASS most likely to appear must come first among the cases.
                    //
                    //--------------------------------------------------------------------------------
                    switch ((*itr)->getObjectClass()) {
                    //--------------------------------------------------------------------------------
                    // When there is a creature on the tile
                    //--------------------------------------------------------------------------------
                    case Object::OBJECT_CLASS_CREATURE: {
                        Creature* pCreature = dynamic_cast<Creature*>(*itr);
                        Assert(pCreature != NULL);

                        // There is no need to receive one's own information.
                        if (pCreature == pPC)
                            continue;

                        switch (pCreature->getCreatureClass()) {
                        case Creature::CREATURE_CLASS_MONSTER: {
                            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                            //--------------------------------------------------------------------------------
                            //
                            // If this monster was invisible from the previous coordinate but becomes
                            // visible from the destination coordinate, send a GCAddMonster packet.
                            //
                            //--------------------------------------------------------------------------------
                            if (prevVisionState == OUT_OF_SIGHT && curVisionState >= IN_SIGHT) {
                                Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, pPC);

                                if (pAddMonsterPacket != NULL) {
                                    pPlayer->sendPacket(pAddMonsterPacket);
                                    delete pAddMonsterPacket;
                                }
                            }

                            //--------------------------------------------------------------------------------
                            // Register the PC as a potential enemy of the monster.
                            //--------------------------------------------------------------------------------
                            VisionState vs = pMonster->getVisionState(x2, y2);

                            // Register as an enemy only for Aggressive monsters.
                            if (vs >= IN_SIGHT && pMonster->getAlignment() == ALIGNMENT_AGGRESSIVE) {
                                if (isPotentialEnemy(pMonster, pPC)) {
                                    pMonster->addPotentialEnemy(pPC);
                                }
                            }

                        } break;

                        //--------------------------------------------------------------------------------
                        //
                        //--------------------------------------------------------------------------------
                        case Creature::CREATURE_CLASS_SLAYER: {
                            //--------------------------------------------------------------------------------
                            // Only creatures that were invisible at the previous coordinate and are
                            // visible at this one get a GCAddXXX. Those that stay visible do not.
                            //--------------------------------------------------------------------------------
                            if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                // If the viewer is sniping, it has to have been detected.
                                //												if
                                //(!pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)
                                //													||
                                // pPC->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) )
                                //													|| ( pEffectRevealer != NULL &&
                                // pEffectRevealer->canSeeSniping( pCreature ) ) )
                                if (canSee(pPC, pCreature)) {
                                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                                    //													GCAddSlayer
                                    GCAddSlayer gcAddSlayer;
                                    makeGCAddSlayer(&gcAddSlayer, pSlayer);

                                    pPlayer->sendPacket(&gcAddSlayer);
                                }
                            }

                            Assert(pCreature->getPlayer() != NULL);

                            //--------------------------------------------------------------------------------
                            //
                            // Send the GCAddXXX packet only when Q(x2,y2) is on the border of this
                            // creature's sight rectangle and P(x1,y1) is outside it, i.e. invisible.
                            // Otherwise the server would keep sending GCAddXXX packets as the PC keeps
                            // moving along pCreature's sight border.
                            //
                            // In summary,
                            //
                            // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                            // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                            //
                            //--------------------------------------------------------------------------------
                            VisionState prevVS = pCreature->getVisionState(x1, y1);
                            VisionState currVS = pCreature->getVisionState(x2, y2);

                            // Entering the visible interior straight from the invisible area without
                            // passing through the border area is impossible.

                            if (canSee(pCreature, pPC)) {
                                if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                }
                            }
                        } break;

                        case Creature::CREATURE_CLASS_VAMPIRE: {
                            //--------------------------------------------------------------------------------
                            // Only creatures that were invisible at the previous coordinate and are
                            // visible at this one get a GCAddXXX. If it was NEW_SIGHT before and is
                            // NEW_SIGHT now, nothing new is fetched.
                            //--------------------------------------------------------------------------------
                            if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
                                if (canSee(pPC, pCreature)) {
                                    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) {
                                        {
                                            GCAddBurrowingCreature gcABC;
                                            gcABC.setObjectID(pCreature->getObjectID());
                                            gcABC.setName(pCreature->getName());
                                            gcABC.setX(ix);
                                            gcABC.setY(iy);
                                            pPlayer->sendPacket(&gcABC);
                                        }
                                    } else {
                                        //													if
                                        //(!pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY))
                                        //													{
                                        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                                        //															GCAddVampire
                                        GCAddVampire gcAddVampire;
                                        makeGCAddVampire(&gcAddVampire, pVampire);
                                        pPlayer->sendPacket(&gcAddVampire);
                                        // pCreature is in the invisibility state.
                                        // pPC->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY)
                                    }
                                }
                            }

                            Assert(pCreature->getPlayer() != NULL);

                            //--------------------------------------------------------------------------------
                            // Send the GCAddXXX packet only when Q(x2,y2) is on the border of this
                            // creature's sight rectangle and P(x1,y1) is outside it, i.e. invisible.
                            // Otherwise the server would keep sending GCAddXXX packets as the PC keeps
                            // moving along pCreature's sight border.
                            //
                            // In summary,
                            //
                            // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                            // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                            //
                            //--------------------------------------------------------------------------------
                            VisionState prevVS = pCreature->getVisionState(x1, y1);
                            VisionState currVS = pCreature->getVisionState(x2, y2);

                            // The other party is a Vampire, so my darkness state does not matter.
                            // Hide does not matter either.
                            // *NOTE
                            // If the other party is a Slayer, whether it is sniping has to be checked.
                            if (canSee(pCreature, pPC)) {
                                if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                }
                            }
                        } break;

                        case Creature::CREATURE_CLASS_OUSTERS: {
                            //--------------------------------------------------------------------------------
                            // Only creatures that were invisible at the previous coordinate and are
                            // visible at this one get a GCAddXXX. If it was NEW_SIGHT before and is
                            // NEW_SIGHT now, nothing new is fetched.
                            //--------------------------------------------------------------------------------
                            if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT &&
                                canSee(pPC, pCreature)) {
                                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                                //												GCAddOusters
                                GCAddOusters gcAddOusters;
                                makeGCAddOusters(&gcAddOusters, pOusters);
                                pPlayer->sendPacket(&gcAddOusters);
                            }

                            Assert(pCreature->getPlayer() != NULL);

                            //--------------------------------------------------------------------------------
                            // Send the GCAddXXX packet only when Q(x2,y2) is on the border of this
                            // creature's sight rectangle and P(x1,y1) is outside it, i.e. invisible.
                            // Otherwise the server would keep sending GCAddXXX packets as the PC keeps
                            // moving along pCreature's sight border.
                            //
                            // In summary,
                            //
                            // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                            // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                            //
                            //--------------------------------------------------------------------------------
                            VisionState prevVS = pCreature->getVisionState(x1, y1);
                            VisionState currVS = pCreature->getVisionState(x2, y2);

                            // The other party is an Ousters, so my darkness state does not matter.
                            // Hide does not matter either.
                            // *NOTE
                            // If the other party is a Slayer, whether it is sniping has to be checked.
                            if (canSee(pCreature, pPC)) {
                                if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(pGCAddXXX);
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                }
                            }
                        } break;

                        case Creature::CREATURE_CLASS_NPC: {
                            NPC* pNPC = dynamic_cast<NPC*>(pCreature);

                            //--------------------------------------------------------------------------------
                            //
                            // If this monster was invisible from the previous coordinate but becomes
                            // visible from the destination coordinate, send a GCAddMonster packet.
                            //
                            //--------------------------------------------------------------------------------
                            if (prevVisionState == OUT_OF_SIGHT && curVisionState >= IN_SIGHT) {
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
                    // When there is an item on the tile
                    //--------------------------------------------------------------------------------
                    case Object::OBJECT_CLASS_ITEM: {
                        if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
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
                    } break;

                    //--------------------------------------------------------------------------------
                    // When there is an effect on the tile
                    //--------------------------------------------------------------------------------
                    case Object::OBJECT_CLASS_EFFECT: {
                        Effect* pEffect = dynamic_cast<Effect*>(*itr);

                        if (curVisionState >= IN_SIGHT && prevVisionState == OUT_OF_SIGHT) {
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

                                // For sanctuary the packet is sent only at the centre coordinate.
                                if (centerX == ix && centerY == iy) {
                                    GCAddEffectToTile gcAddEffectToTile;

                                    gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                                    gcAddEffectToTile.setXY(ix, iy);
                                    gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                                    gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                                    pPlayer->sendPacket(&gcAddEffectToTile);
                                }
                            } else if (pEffect->isBroadcastingEffect()) {
                                GCAddEffectToTile gcAddEffectToTile;

                                gcAddEffectToTile.setObjectID(pEffect->getObjectID());
                                gcAddEffectToTile.setXY(ix, iy);
                                gcAddEffectToTile.setEffectID(pEffect->getSendEffectClass());
                                gcAddEffectToTile.setDuration(pEffect->getRemainDuration());

                                pPlayer->sendPacket(&gcAddEffectToTile);
                            }
                        }
                    } break;

                    //--------------------------------------------------------------------------------
                    // When there is an obstacle on the tile
                    //--------------------------------------------------------------------------------
                    case Object::OBJECT_CLASS_OBSTACLE: {
                        // darkness
                    } break;

                    //--------------------------------------------------------------------------------
                    // When there is a portal on the tile
                    //--------------------------------------------------------------------------------
                    case Object::OBJECT_CLASS_PORTAL: {
                        // darkness
                    } break;

                    default:
                        throw Error("invalid object class");

                    } // switch ((*itr)->getObjectClass())
                } while (++itr != objectList.end()); // do ~ while
            } // if
        } // for
    } // for

    SAFE_DELETE(pGCAddXXX);

    return true;

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Broadcasts a creature's fast move from P(x1,y1) to Q(x2,y2) to the PCs
// in the surrounding area.
// for Skill FlashSliding, ShadowWalk
//////////////////////////////////////////////////////////////////////////////
bool Zone::moveFastMonster(Monster* pMonster, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2,
                           SkillType_t skillType) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    if (
        /*		pMonster->isFlag(Effect::EFFECT_CLASS_PARALYZE)
                || pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)
                || pMonster->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS)
                || pMonster->isFlag(Effect::EFFECT_CLASS_SLEEP)
                || pMonster->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) */
        !isAbleToMove(pMonster)) {
        // do nothing
        return false;
    }

    ZoneLevel_t ZoneLevel = getZoneLevel(x2, y2);

    // Safe zones cannot be entered.
    if ((ZoneLevel & SLAYER_SAFE_ZONE) || (ZoneLevel & VAMPIRE_SAFE_ZONE) || (ZoneLevel & COMPLETE_SAFE_ZONE)) {
        return false;
    }

    // Find a suitable destination point.
    // Four candidate points in front are searched.
    Dir_t dir = calcDirection(x1, y1, x2, y2);

    // g_FastMoveSearchX, Y are used for the search.
    int* searchX = g_FastMoveSearchX[dir];
    int* searchY = g_FastMoveSearchY[dir];

    // Check whether the tile is empty.
    int i = 0;
    for (i = 0; i < 4; i++) {
        int targetX = x2 + searchX[i], targetY = y2 + searchY[i];
        if (targetX >= 0 && targetX < m_Width && targetY >= 0 && targetY < m_Height &&
            !m_pTiles[targetX][targetY].isBlocked(pMonster->getMoveMode()) && !m_pTiles[targetX][targetY].hasPortal()) {
            x2 = targetX;
            y2 = targetY;
            break;
        }
    }
    if (i == 4) {
        return false; // No empty tile was found!
    }

    // Build the packet first and send it below.
    GCFastMove gcFastMove;
    gcFastMove.setObjectID(pMonster->getObjectID());
    gcFastMove.setXY(x1, y1, x2, y2);
    gcFastMove.setSkillType(skillType);

#ifdef __USE_ENCRYPTER__
    SocketEncryptOutputStream outputStream(NULL, szPacketHeader + gcFastMove.getPacketSize() + 2);
    outputStream.setEncryptCode(m_EncryptCode);
#else
    SocketOutputStream outputStream(NULL, szPacketHeader + gcFastMove.getPacketSize() + 2);
#endif
    gcFastMove.writeHeaderNBody(outputStream);

    // There is no need to send it to the monster.

    //////////////////////////////////////////////////////////////
    // The kind of move....
    // Depending on it, GCDelete or Add may have to be sent.

    // Change the Monster's coordinates.
    pMonster->setXYDir(x2, y2, dir);
    // Delete the creature from the previous tile.

    try {
        m_pTiles[x1][y1].deleteCreature(pMonster->getObjectID());
    } catch (Error& e) {
        filelog("assertTile.txt", "moveFastMonster : %s", e.toString().c_str());
        throw;
    }

    // Add the creature to the new tile.
    m_pTiles[x2][y2].addCreature(pMonster);

    try {
        checkMine(this, pMonster, x2, y2);
        checkTrap(this, pMonster);
    } catch (Throwable& t) {
        diagnosticFilelog("CheckMineBug.txt", "%s : %s", "moveFastMonster", t.toString().c_str());
    }


    //--------------------------------------------------------------------------------
    // Prepare the monster's add packet: GCAddMonster, or the bat, wolf or
    // burrowing variant its flags select.
    // By the current policy, the GCAdd packet is based on the current coordinates.
    //--------------------------------------------------------------------------------
    Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, NULL);

    if (pAddMonsterPacket != NULL) {
        //--------------------------------------------------------------------------------
        // Prepare the GCDeleteObject packet.
        //--------------------------------------------------------------------------------
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pMonster->getObjectID());


        // Compute the total range of sight.
        ZoneCoord_t minX, maxX, minY, maxY;
        if (x1 < x2) {
            minX = max(0, x1 - maxViewportWidth);
            maxX = min(m_Width - 1, x2 + maxViewportWidth);
        } else {
            minX = max(0, x2 - maxViewportWidth);
            maxX = min(m_Width - 1, x1 + maxViewportWidth);
        }
        if (y1 < y2) {
            minY = max(0, y1 - maxViewportUpperHeight);
            maxY = min(m_Height - 1, y2 + maxViewportLowerHeight);
        } else {
            minY = max(0, y2 - maxViewportUpperHeight);
            maxY = min(m_Height - 1, y1 + maxViewportLowerHeight);
        }


        for (ZoneCoord_t ix = minX; ix <= maxX; ix++) {
            for (ZoneCoord_t iy = minY; iy <= maxY; iy++) {
                const forward_list<Object*>& objectList = m_pTiles[ix][iy].getObjectList();

                forward_list<Object*>::const_iterator itr = objectList.begin();

                // Because of visionInfo,
                // the structure is an if plus a do-while rather than a plain if.
                if (itr != objectList.end()) {
                    // How is I(ix,iy) seen from the previous coordinate P(x1,y1)?
                    // How is I(ix,iy) seen from the current coordinate Q(x2,y2)?

                    do {
                        Assert(*itr != NULL);

                        //--------------------------------------------------------------------------------
                        //
                        // Build the GCAddXXX packet matching each object's OBJECT CLASS and
                        // send it to the owner.
                        //
                        // *NOTES*
                        //
                        // The object CLASS most likely to appear must come first among the cases.
                        //
                        //--------------------------------------------------------------------------------
                        switch ((*itr)->getObjectClass()) {
                        //--------------------------------------------------------------------------------
                        // When there is a creature on the tile
                        //--------------------------------------------------------------------------------
                        case Object::OBJECT_CLASS_CREATURE: {
                            Creature* pCreature = dynamic_cast<Creature*>(*itr);
                            Assert(pCreature != NULL);

                            // There is no need to receive one's own information.
                            if (pCreature == pMonster)
                                continue;

                            switch (pCreature->getCreatureClass()) {
                            case Creature::CREATURE_CLASS_MONSTER: {
                                Monster* pOtherMonster = dynamic_cast<Monster*>(pCreature);

                                //--------------------------------------------------------------------------------
                                // Register the two monsters as potential enemies of each other.
                                //--------------------------------------------------------------------------------

                                {
                                    if (isPotentialEnemy(pOtherMonster, pMonster)) {
                                        pMonster->addPotentialEnemy(pOtherMonster);
                                        pOtherMonster->addPotentialEnemy(pMonster);
                                    }
                                }
                            } break;

                            //--------------------------------------------------------------------------------
                            //
                            //--------------------------------------------------------------------------------
                            case Creature::CREATURE_CLASS_SLAYER: {
                                Assert(pCreature->getPlayer() != NULL);

                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // If the Creature has the ObservingEye effect, get it.
                                //												EffectObservingEye* pEffectObservingEye
                                //													//Assert( pEffectObservingEye !=

                                // Packet announcing the monster's appearance to the other party.
                                //												if
                                //((!pMonster->isFlag(Effect::EFFECT_CLASS_HIDE) ||
                                // pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) ) //|| (
                                // pEffectRevealerCreature
                                //!= NULL && pEffectRevealerCreature->canSeeHide( pPC ) ) )
                                //													&&
                                //(!pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) ||
                                // pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) || ( pEffectObservingEye
                                //!= NULL && pEffectObservingEye->canSeeInvisibility( pMonster ) ) )
                                //													&&
                                //(!pMonster->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
                                // pCreature->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) )) //|| (
                                // pEffectRevealerCreature != NULL && pEffectRevealerCreature->canSeeSniping( pPC) ) ) )
                                if (canSee(pCreature, pMonster)) {
                                    if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(pAddMonsterPacket);
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                    }
                                }
                            } break;

                            case Creature::CREATURE_CLASS_VAMPIRE: {
                                Assert(pCreature->getPlayer() != NULL);

                                //--------------------------------------------------------------------------------
                                // Send the GCAddXXX packet only when Q(x2,y2) is on the border of this
                                // creature's sight rectangle and P(x1,y1) is outside it, i.e. invisible.
                                // Otherwise the server would keep sending GCAddXXX packets as the PC keeps
                                // moving along pCreature's sight border.
                                //
                                // In summary,
                                //
                                // OUT_OF_SIGHT -> ON_SIGHT/NEW_SIGHT : GCAddXXX
                                // IN_SIGHT/ON_SIGHT/NEW_SIGHT -> IN_SIGHT/ON_SIGHT/NEW_SIGHT : GCMove
                                //
                                //--------------------------------------------------------------------------------
                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // The other party is a Vampire, so my darkness state does not matter.
                                // Hide does not matter either.
                                // *NOTE
                                // If the other party is a Slayer, whether it is sniping has to be checked.
                                if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(pAddMonsterPacket);
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                    pCreature->getPlayer()->sendStream(&outputStream);
                                } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                    pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                }
                            } break;

                            case Creature::CREATURE_CLASS_OUSTERS: {
                                Assert(pCreature->getPlayer() != NULL);

                                VisionState prevVS = pCreature->getVisionState(x1, y1);
                                VisionState currVS = pCreature->getVisionState(x2, y2);

                                // Packet announcing the monster's appearance to the other party.
                                if (canSee(pCreature, pMonster)) {
                                    if (prevVS == OUT_OF_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(pAddMonsterPacket);
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS >= IN_SIGHT) {
                                        pCreature->getPlayer()->sendStream(&outputStream);
                                    } else if (prevVS >= IN_SIGHT && currVS == OUT_OF_SIGHT) {
                                        pCreature->getPlayer()->sendPacket(&gcDeleteObject);
                                    }
                                }
                            } break;
                            case Creature::CREATURE_CLASS_NPC: {
                            } break;

                            default:
                                throw Error("invalid creature class");

                            } // switch (pCreature->getCreatureClass())

                        } // case Object::OBJECT_CLASS_CREATURE :

                        break;

                        //--------------------------------------------------------------------------------
                        // When there is an item on the tile
                        //--------------------------------------------------------------------------------
                        case Object::OBJECT_CLASS_ITEM: {
                        } break;

                        //--------------------------------------------------------------------------------
                        // When there is an effect on the tile
                        //--------------------------------------------------------------------------------
                        case Object::OBJECT_CLASS_EFFECT: {
                        } break;

                        //--------------------------------------------------------------------------------
                        // When there is an obstacle on the tile
                        //--------------------------------------------------------------------------------
                        case Object::OBJECT_CLASS_OBSTACLE: {
                            // darkness
                        } break;

                        //--------------------------------------------------------------------------------
                        // When there is a portal on the tile
                        //--------------------------------------------------------------------------------
                        case Object::OBJECT_CLASS_PORTAL: {
                            // darkness
                        } break;

                        default:
                            throw Error("invalid object class");

                        } // switch ((*itr)->getObjectClass())
                    } while (++itr != objectList.end()); // do ~ while
                } // if
            } // for
        } // for


        delete pAddMonsterPacket;
    }

    return true;

    __END_DEBUG
    __END_CATCH
}
