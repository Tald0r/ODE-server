//////////////////////////////////////////////////////////////////////////////
// Filename    : ZoneUtil.cpp
// Written by  : excel96
// Description :
// Functions that perform zone-related work were pulled out of the zone
// file because putting them inside it made that file too large.
//////////////////////////////////////////////////////////////////////////////

#include "ZoneUtil.h"

#include "Assert.h"
#include "CastleInfoManager.h"
#include "Corpse.h"
#include "CreatureUtil.h"
#include "Effect.h"
#include "EffectDarknessForbidden.h"
#include "EffectGnomesWhisper.h"
#include "EffectManager.h"
#include "EffectObservingEye.h"
#include "EffectPrecedence.h"
#include "EventTransport.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddInstalledMineToZone.h"
#include "GCAddMonster.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddSlayer.h"
#include "GCAddVampire.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCDeleteInventoryItem.h"
#include "GCDeleteObject.h"
#include "GCDropItemToZone.h"
#include "GCFastMove.h"
#include "GCGetOffMotorCycle.h"
#include "GCMineExplosionOK1.h"
#include "GCMineExplosionOK2.h"
#include "GCMove.h"
#include "GCMoveError.h"
#include "GCMoveOK.h"
#include "GCRemoveEffect.h"
#include "GCSetPosition.h"
#include "GCSystemMessage.h"
#include "GCUnburrowFail.h"
#include "GCUnburrowOK.h"
#include "GCUntransformFail.h"
#include "GCUntransformOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "IncomingPlayerManager.h"
#include "Item.h"
#include "KernelContext.h"
#include "LevelWarZoneInfoManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "MonsterSummonInfo.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "ServerContext.h"
#include "SkillHandler.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "StringPool.h"
#include "TimeManager.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "VisionInfo.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneGroupManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "item/Mine.h"
#include "repository/BulletinBoardRepository.h"
#include "skill/EffectTrapInstalled.h"
#include "skill/SummonGroundElemental.h"
#include "war/WarSystem.h"

string correctString(const string& str) {
    __BEGIN_TRY

    string correct = str;

    unsigned int i = 0;
    unsigned int size = str.size();

    while (i < size) {
        if (correct[i] == '\\') {
            correct.replace(i, 1, "\\\\");
            i = i + 2;
            size++;
        } else if (correct[i] == '\'') {
            correct.replace(i, 1, "\\'");
            i = i + 2;
            size++;
        } else {
            i++;
        }
    }

    return correct;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Find a position where a given creature can be added.
//
// Zone*       pZone        : pointer to the zone
// ZoneCoord_t cx           : initial x position to add at
// ZoneCoord_t cy           : initial y position to add at
// Creature::MoveMode MMode : the creature's move mode
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePosition(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, Creature::MoveMode MMode)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    int x = cx;
    int y = cy;
    int sx = 1;
    int sy = 0;
    int maxCount = 1;
    int count = 1;
    int checkCount = 300;
    TPOINT pt;

    do {
        if (x > 0 && y > 0 && x < pZone->getWidth() && y < pZone->getHeight()) {
            Tile& rTile = pZone->getTile(x, y);
            if (rTile.isBlocked(MMode) == false && rTile.hasPortal() == false) {
                pt.x = x;
                pt.y = y;
                return pt;
            }
        }

        x += sx;
        y += sy;

        if (--count == 0) {
            if (sx == 0)
                maxCount++;

            int temp = sx;
            sx = -sy;
            sy = temp;

            count = maxCount;
        }

    } while (--checkCount);

    pt.x = -1;
    pt.y = -1;
    return pt;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Find a position where a given item can be added.
//
// Zone*       pZone          : pointer to the zone
// ZoneCoord_t cx             : initial x position to add at
// ZoneCoord_t cy             : initial y position to add at
// bool        bAllowCreature : is a tile holding a creature acceptable?
// bool        bAllowSafeZone : is a Safe Zone acceptable?
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePositionForItem(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature,
                                   bool bAllowSafeZone, bool bForce)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    int x = cx;
    int y = cy;
    int sx = 1;
    int sy = 0;
    int maxCount = 1;
    int count = 1;
    int checkCount = 300;
    TPOINT pt;

    do {
        // To keep the item off the edge of the screen, check whether it
        // can be dropped with a certain offset applied.
        if (x > 2 && y > 2 && x < pZone->getWidth() - 2 && y < pZone->getHeight() - 2) {
            Tile& rTile = pZone->getTile(x, y);

            // Not a GroundBlock, or (even if blocked) a ground character is present.
            if ((!rTile.isGroundBlocked() || rTile.hasWalkingCreature()) && rTile.hasItem() == false &&
                rTile.hasPortal() == false) {
                // Check for an item that must not be dropped in a Safe zone.
                if (bAllowSafeZone || !(pZone->getZoneLevel(x, y) & SAFE_ZONE)) {
                    pt.x = x;
                    pt.y = y;
                    return pt;
                }
                // When a motorcycle is created it can end up under an NPC,
                // where the player cannot click it.
                // To avoid that, check whether a creature is on the tile.
                // Either way this passes unconditionally; the code is odd.
            }

            if (bForce && rTile.hasItem()) {
                Item* pTileItem = rTile.getItem();

                if (pTileItem != NULL) {
                    if (pTileItem->getItemClass() != Item::ITEM_CLASS_CORPSE) {
                        pZone->deleteItem(pTileItem, x, y);
                        pTileItem->destroy();
                        SAFE_DELETE(pTileItem);

                        pt.x = x;
                        pt.y = y;
                        return pt;
                    }
                }
            }
        }

        x += sx;
        y += sy;

        if (--count == 0) {
            if (sx == 0)
                maxCount++;

            int temp = sx;
            sx = -sy;
            sy = temp;

            count = maxCount;
        }

    } while (--checkCount);

    pt.x = -1;
    pt.y = -1;
    return pt;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Find a position where a given effect can be added.
//
// Zone*       pZone          : pointer to the zone
// ZoneCoord_t cx             : initial x position to add at
// ZoneCoord_t cy             : initial y position to add at
// Effect::EffectClass EClass : the effect class to add
//////////////////////////////////////////////////////////////////////////////
TPOINT findSuitablePositionForEffect(Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy, Effect::EffectClass EClass)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    int x = cx;
    int y = cy;
    int sx = 1;
    int sy = 0;
    int maxCount = 1;
    int count = 1;
    int checkCount = 300;
    TPOINT pt;

    do {
        if (x > 0 && y > 0 && x < pZone->getWidth() && y < pZone->getHeight()) {
            Tile& rTile = pZone->getTile(x, y);
            // The tile must accept an effect and must not already hold the same kind.
            if (rTile.canAddEffect() && rTile.getEffect(EClass) == NULL) {
                bool bNearTileCheck = true;

                // The 8 surrounding tiles must not hold the same effect.
                for (int i = 0; i < 8; i++) {
                    int tileX = x + dirMoveMask[i].x;
                    int tileY = y + dirMoveMask[i].y;

                    if (pZone->getOuterRect()->ptInRect(tileX, tileY)) {
                        Tile& rTile2 = pZone->getTile(tileX, tileY);
                        if (rTile2.getEffect(EClass) != NULL) {
                            bNearTileCheck = false;
                            break;
                        }
                    }
                }

                if (bNearTileCheck) {
                    pt.x = x;
                    pt.y = y;
                    return pt;
                }
            }
        }

        x += sx;
        y += sy;

        if (--count == 0) {
            if (sx == 0)
                maxCount++;

            int temp = sx;
            sx = -sy;
            sy = temp;

            count = maxCount;
        }

    } while (--checkCount);

    pt.x = -1;
    pt.y = -1;

    return pt;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Check whether a creature with the given move mode can be added at a position.
//
// Zone*              pZone : pointer to the zone
// ZoneCoord_t        x     : x coordinate to check
// ZoneCoord_t        y     : y coordinate to check
// Creature::MoveMode MMode : the creature's move mode
//////////////////////////////////////////////////////////////////////////////
bool canAddCreature(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Creature::MoveMode MMode)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    if (x > 0 && y > 0 && x < pZone->getWidth() && y < pZone->getHeight()) {
        if (!pZone->getTile(x, y).isBlocked(MMode)) {
            return true;
        }
    }

    return false;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Check whether burrowing is possible at a position.
//
// Zone*       pZone : pointer to the zone
// ZoneCoord_t x     : x coordinate to burrow at
// ZoneCoord_t y     : y coordinate to burrow at
//////////////////////////////////////////////////////////////////////////////
bool canBurrow(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    return canAddCreature(pZone, x, y, Creature::MOVE_MODE_BURROWING);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Check whether unburrowing is possible at a position.
//
// Zone*       pZone : pointer to the zone
// ZoneCoord_t x     : x coordinate to unburrow at
// ZoneCoord_t y     : y coordinate to unburrow at
//////////////////////////////////////////////////////////////////////////////
bool canUnburrow(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    return canAddCreature(pZone, x, y, Creature::MOVE_MODE_WALKING);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Push a creature backwards.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature to push back
// ZoneCoord_t originX   : x coordinate of the opponent pushing pCreature back
// ZoneCoord_t originY   : y coordinate of the opponent pushing pCreature back
//////////////////////////////////////////////////////////////////////////////
Dir_t knockbackCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t originX, ZoneCoord_t originY) {
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    if (pCreature->isDead() || pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE) || pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
        return UP;
    }

    if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        if (pMonster != NULL && pMonster->getMonsterType() == GROUND_ELEMENTAL_TYPE)
            return UP;
        switch (pMonster->getMonsterType()) {
        case 722:
        case 723:
        case 717:
        case 721:
        case 724:
        case 725:
        case 726:
        case 727:
        case 728:
        case 729:
        case 734:
        case 735:
        case 736:
        case 737:
        case 764:
        case 765:
            return UP;
        default:
            break;
        }
    }

    // Compute the coordinates and direction the creature retreats to.
    ZoneCoord_t nx = pCreature->getX();
    ZoneCoord_t ny = pCreature->getY();
    ZoneCoord_t cx = nx;
    ZoneCoord_t cy = ny;
    ZoneCoord_t height = pZone->getHeight();
    ZoneCoord_t width = pZone->getWidth();
    Dir_t dir = calcDirection(originX, originY, nx, ny);

    Tile& rOriginTile = pZone->getTile(cx, cy);
    if (rOriginTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL)
        return UP;

    // Compute the retreat coordinates.
    switch (dir) {
    case UP:
        if (ny > 0) {
            ny -= 1;
        }
        break;
    case DOWN:
        if (ny < (height - 1)) {
            ny += 1;
        }
        break;
    case LEFT:
        if (nx > 0) {
            nx -= 1;
        }
        break;
    case RIGHT:
        if (nx < (width - 1)) {
            nx += 1;
        }
        break;
    case LEFTUP:
        if (nx > 0 && ny > 0) {
            nx -= 1;
            ny -= 1;
        }
        break;
    case RIGHTUP:
        if (nx < (width - 1) && ny > 0) {
            nx += 1;
            ny -= 1;
        }
        break;
    case LEFTDOWN:
        if (nx > 0 && ny < (height - 1)) {
            nx -= 1;
            ny += 1;
        }
        break;
    case RIGHTDOWN:
        if (nx < (width - 1) && ny < (height - 1)) {
            nx += 1;
            ny += 1;
        }
        break;
    }

    // The creature must not be in a casket, the destination must be
    // empty, and the creature must be able to move.
    Tile& rTargetTile = pZone->getTile(nx, ny);
    if (!pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) && !rTargetTile.isBlocked(pCreature->getMoveMode()) &&
        !pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) && !rTargetTile.hasPortal()) {
        pCreature->setX(nx);
        pCreature->setY(ny);

        try {
            // Take the creature off the tile it stood on.
            pZone->deleteCreatureFromTile(pCreature, cx, cy);

            // Put it on the new tile.
            if (!pZone->addCreatureToTile(pCreature, nx, ny)) {
                // A Portal was activated.
                return dir;
            }

            // Check for a mine.
            try {
                checkMine(pZone, pCreature, nx, ny);
                checkTrap(pZone, pCreature);
            } catch (Throwable& t) {
                filelog("CheckMineBug.txt", "%s : %s", "KnockBackCreature", t.toString().c_str());
            }

            // Broadcast GCMove/GCAddSlayer/GCAddVampire.
            if (pCreature->isPC()) {
                pZone->movePCBroadcast(pCreature, cx, cy, nx, ny, false, true);
            } else {
                pZone->moveCreatureBroadcast(pCreature, cx, cy, nx, ny, false, true);
            }
        } catch (NoSuchElementException& nsee) {
            throw Error("No creature on previous tile");
        } catch (DuplicatedException& de) {
            throw Error("Thers's a creature on new tile");
        } catch (PortalException&) {
            // Used as a jump out of the block.
        } catch (Error& e) {
            filelog("assertTile.txt", "knockbackCreature : %s", e.toString().c_str());
            throw;
        }
    }

    return dir;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Add a creature that used Hide to the zone.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that used Hide
// ZoneCoord_t cx        : the creature's original x coordinate
// ZoneCoord_t cy        : the creature's original y coordinate
//////////////////////////////////////////////////////////////////////////////
void addBurrowingCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy) {
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);
    Assert(pCreature->isVampire() || pCreature->isMonster());

    TPOINT pt = findSuitablePosition(pZone, cx, cy, Creature::MOVE_MODE_BURROWING);

    if (pt.x != -1) {
        pCreature->setFlag(Effect::EFFECT_CLASS_HIDE);
        Assert(pCreature->getMoveMode() == Creature::MOVE_MODE_WALKING);

        try {
            pZone->deleteCreatureFromTile(pCreature, pCreature->getX(), pCreature->getY());
        } catch (Error& e) {
            filelog("assertTile.txt", "addBurrowingCreature : %s", e.toString().c_str());
            throw;
        }

        // A tile files a creature under its move mode, so the mode is changed by
        // taking the creature off its tile and adding it again.
        pCreature->setMoveMode(Creature::MOVE_MODE_BURROWING);
        pZone->addCreatureToTile(pCreature, pt.x, pt.y);

        Assert(pCreature == pZone->getTile(pt.x, pt.y).getCreature(pCreature->getMoveMode()));

        // Set the creature's coordinates.
        pCreature->setXYDir(pt.x, pt.y, pCreature->getDir());


        // GCAddBurrowingCreature to notify the nearby PCs.
        GCAddBurrowingCreature gcABC;
        gcABC.setObjectID(pCreature->getObjectID());
        gcABC.setName(pCreature->getName());
        gcABC.setX(pt.x);
        gcABC.setY(pt.y);

        //--------------------------------------------------------------------------------
        //
        // Enlarge the sight area by 1 on every side.
        // The ON_SIGHT area grows with the direction.
        //
        //--------------------------------------------------------------------------------


        // broadcastPacket itself handles whether the packet can be seen.
        pZone->broadcastPacket(pt.x, pt.y, &gcABC, pCreature);
    } else
        throw EmptyTileNotExistException("addBurrowingCreature() : Tile is not empty.");

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Add a creature that left Hide to the zone.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that left Hide
// ZoneCoord_t cx        : the creature's original x coordinate
// ZoneCoord_t cy        : the creature's original y coordinate
// Dir_t       dir       : the direction the emerging creature faces
//////////////////////////////////////////////////////////////////////////////
void addUnburrowCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir) {
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);
    Assert(pCreature->isFlag(Effect::EFFECT_CLASS_HIDE));

    TPOINT pt = findSuitablePosition(pZone, cx, cy, Creature::MOVE_MODE_WALKING);

    if (pt.x != -1) {
        ZoneCoord_t oldX = pCreature->getX();
        ZoneCoord_t oldY = pCreature->getY();

        // Send Delete object to anyone who could see it at the old position despite the hide.
        GCDeleteObject gcDO;
        gcDO.setObjectID(pCreature->getObjectID());
        pZone->broadcastPacket(oldX, oldY, &gcDO, pCreature);

        // Set after sending the DeleteObject packet.
        pCreature->removeFlag(Effect::EFFECT_CLASS_HIDE);

        // Take the creature off its old tile and add it to the new one with a
        // changed move mode: a tile files a creature under its move mode.
        try {
            pZone->deleteCreatureFromTile(pCreature, oldX, oldY);
        } catch (Error& e) {
            filelog("assertTile.txt", "addUnburrowCreature : %s", e.toString().c_str());
            throw;
        }
        pCreature->setMoveMode(Creature::MOVE_MODE_WALKING);
        pZone->addCreatureToTile(pCreature, pt.x, pt.y);

        Assert(pCreature == pZone->getTile(pt.x, pt.y).getCreature(pCreature->getMoveMode()));

        // Set the creature's coordinates.
        pCreature->setXYDir(pt.x, pt.y, dir);


        Creature::CreatureClass CClass = pCreature->getCreatureClass();
        if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
            // Have the nearby PCs add the vampire.
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            GCAddVampireFromBurrowing gcAVFB(pVampire->getVampireInfo3());
            gcAVFB.setEffectInfo(pVampire->getEffectInfo());
            pZone->broadcastPacket(pt.x, pt.y, &gcAVFB, pCreature);

            // Tell the creature itself that it came out of the ground.
            GCUnburrowOK gcUnburrowOK(pt.x, pt.y, dir);
            Player* pPlayer = pCreature->getPlayer();
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
            pGamePlayer->sendPacket(&gcUnburrowOK);
        } else if (CClass == Creature::CREATURE_CLASS_MONSTER) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            GCAddMonsterFromBurrowing gcAMFB;
            gcAMFB.setObjectID(pMonster->getObjectID());
            gcAMFB.setMonsterType(pMonster->getMonsterType());
            gcAMFB.setMonsterName(pMonster->getMonsterName());
            gcAMFB.setX(pt.x);
            gcAMFB.setY(pt.y);
            gcAMFB.setDir(dir);
            gcAMFB.setEffectInfo(pMonster->getEffectInfo());
            gcAMFB.setCurrentHP(pMonster->getHP());
            gcAMFB.setMaxHP(pMonster->getHP(ATTR_MAX));

            pZone->broadcastPacket(pt.x, pt.y, &gcAMFB);
        } else {
            throw Error("invalid creature type");
        }
    } else {
        // No suitable place was found, so tell the creature itself
        // that it could not come out of the ground.
        if (pCreature->isPC()) {
            GCUnburrowFail gcUnburrowFail;
            pCreature->getPlayer()->sendPacket(&gcUnburrowFail);
        } else {
            cerr << "addUnburrowCreature() : Cannot find suitable position" << endl;
            throw Error("Cannot unburrow monster.");
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Add a creature that ended its transformation to the zone.
//
// Zone*     pZone     : pointer to the zone
// Creature* pCreature : the creature that ended its transformation
// bool      bForce    : is the transformation being ended by force
//                       before the effect's duration expired?
//////////////////////////////////////////////////////////////////////////////
void addUntransformCreature(Zone* pZone, Creature* pCreature, bool bForce) {
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    // Check that the creature really is transformed.
    Assert(pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
           pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
           pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) ||
           pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF));

    ZoneCoord_t cx = pCreature->getX();
    ZoneCoord_t cy = pCreature->getY();

    TPOINT pt = findSuitablePosition(pZone, cx, cy, Creature::MOVE_MODE_WALKING);

    if (pt.x != -1) {
        Range_t rangeDiff = 0;

        if (pt.x != cx || pt.y != cy) {
            rangeDiff = max(abs((int)(pt.x) - (int)(cx)), abs((int)(pt.y) - (int)(cy)));
        }

        ZoneCoord_t oldX = pCreature->getX();
        ZoneCoord_t oldY = pCreature->getY();

        GCDeleteObject gcDO;
        gcDO.setObjectID(pCreature->getObjectID());
        pZone->broadcastPacket(oldX, oldY, &gcDO, pCreature);

        // Delete the effect.
        EffectManager* pEffectManager = pCreature->getEffectManager();
        Assert(pEffectManager != NULL);

        if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
            pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF); // Set after the DeleteObject packet.

            // To delete the effect before its duration expires,
            // deleteEffect must be called explicitly.
            if (bForce) {
                // Send RemoveEffect.
                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(pCreature->getObjectID());
                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);
                if (pCreature->isPC()) {
                    Player* pPlayer = pCreature->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcRemoveEffect);
                }

                pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);
            }

            if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                VAMPIRE_RECORD prev;

                pVampire->getVampireRecord(prev);
                pVampire->initAllStat();
                pVampire->sendModifyInfo(prev);
            } else if (pCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                pMonster->initAllStat();
            }
        }
        if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
            pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF); // Set after the DeleteObject packet.

            // To delete the effect before its duration expires,
            // deleteEffect must be called explicitly.
            if (bForce) {
                // Send RemoveEffect.
                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(pCreature->getObjectID());
                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);
                if (pCreature->isPC()) {
                    Player* pPlayer = pCreature->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcRemoveEffect);
                }

                pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF);
            }

            if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                VAMPIRE_RECORD prev;

                pVampire->getVampireRecord(prev);
                pVampire->initAllStat();
                pVampire->sendModifyInfo(prev);
            } else if (pCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                pMonster->initAllStat();
            }
        } else if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
            pCreature->removeFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT); // Set after the DeleteObject packet.

            // To delete the effect before its duration expires,
            // deleteEffect must be called explicitly.
            if (bForce) {
                // Send RemoveEffect.
                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(pCreature->getObjectID());
                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);
                if (pCreature->isPC()) {
                    Player* pPlayer = pCreature->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcRemoveEffect);
                }

                pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);
            }

            if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                VAMPIRE_RECORD prev;

                pVampire->getVampireRecord(prev);
                pVampire->initAllStat();
                pVampire->sendModifyInfo(prev);
            } else if (pCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pCreature);
                pMonster->initAllStat();
            }
        }

        // Take the creature off its old tile, change its move mode, and add it to
        // the new tile: a tile files a creature under its move mode.
        try {
            pZone->deleteCreatureFromTile(pCreature, oldX, oldY);
        } catch (Error& e) {
            filelog("assertTile.txt", "addUntransformCreature : %s", e.toString().c_str());
            throw;
        }
        pCreature->setMoveMode(Creature::MOVE_MODE_WALKING);
        pZone->addCreatureToTile(pCreature, pt.x, pt.y);

        Assert(pCreature == pZone->getTile(pt.x, pt.y).getCreature(pCreature->getMoveMode()));

        // Set the creature's coordinates.
        pCreature->setXYDir(pt.x, pt.y, pCreature->getDir());

        // Broadcast to the zone according to the creature class.
        Creature::CreatureClass CClass = pCreature->getCreatureClass();

        if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            GCAddVampireFromTransformation gcAVFT(pVampire->getVampireInfo3());
            gcAVFT.setEffectInfo(pVampire->getEffectInfo());

            pZone->broadcastPacket(pt.x, pt.y, &gcAVFT, pVampire, true, rangeDiff);

            // send to myself
            GCUntransformOK gcUntransformOK(pt.x, pt.y, pCreature->getDir());
            pCreature->getPlayer()->sendPacket(&gcUntransformOK);
        } else if (CClass == Creature::CREATURE_CLASS_MONSTER) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            GCAddMonsterFromTransformation gcAMFT;
            gcAMFT.setObjectID(pMonster->getObjectID());
            gcAMFT.setMonsterType(pMonster->getMonsterType());
            gcAMFT.setMonsterName(pMonster->getMonsterName());
            gcAMFT.setX(pt.x);
            gcAMFT.setY(pt.y);
            gcAMFT.setDir(pMonster->getDir());
            gcAMFT.setEffectInfo(pMonster->getEffectInfo());
            gcAMFT.setCurrentHP(pMonster->getHP());
            gcAMFT.setMaxHP(pMonster->getHP(ATTR_MAX));

            pZone->broadcastPacket(pt.x, pt.y, &gcAMFT, NULL, true, rangeDiff);
        } else {
            throw Error("invalid creature type");
        }
    } else {
        if (pCreature->isPC()) {
            GCUntransformFail gcUntransformFail;
            pCreature->getPlayer()->sendPacket(&gcUntransformFail);
        } else {
        }
    }

    // If the creature that untransformed is a vampire, that is a player,
    // send the attack speed. This works around a client bug where the
    // previous attack speed cannot be stored while transformed into a bat.
    if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        GCModifyInformation gcMI;
        gcMI.addShortData(MODIFY_ATTACK_SPEED, pVampire->getAttackSpeed());
        pVampire->getPlayer()->sendPacket(&gcMI);
    }

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Add an invisible creature.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the invisible creature
// ZoneCoord_t cx        : the creature's original x coordinate
// ZoneCoord_t cy        : the creature's original y coordinate
//////////////////////////////////////////////////////////////////////////////
void addInvisibleCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    // Only a vampire or a monster can turn invisible.
    Assert(pCreature->isVampire() || pCreature->isMonster());

    ObjectID_t creatureID = pCreature->getObjectID();

    GCDeleteObject gcDO;
    gcDO.setObjectID(creatureID);

    pCreature->setFlag(Effect::EFFECT_CLASS_INVISIBILITY);


    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(creatureID);
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_INVISIBILITY);
    gcAddEffect.setDuration(0);

    //--------------------------------------------------------------------------------
    //
    // Enlarge the sight area by 1 on every side.
    // The ON_SIGHT area grows with the direction.
    //
    //--------------------------------------------------------------------------------
    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1),
                     endx = min(pZone->getWidth() - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(pZone->getHeight() - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            Tile& curTile = pZone->getTile(ix, iy);
            const forward_list<Object*>& objectList = curTile.getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();
            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                Assert(*itr != NULL);
                Creature* pViewer = dynamic_cast<Creature*>(*itr);

                if (pViewer != pCreature && pViewer->isPC() && (pViewer->getVisionState(cx, cy) >= IN_SIGHT)) {
                    // Take the Viewer's ObservingEye effect.
                    EffectObservingEye* pEffectObservingEye = NULL;
                    if (pViewer->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
                        pEffectObservingEye =
                            dynamic_cast<EffectObservingEye*>(pViewer->findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
                    }

                    // Take the Viewer's Gnome's Whisper effect.
                    EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
                    if (pViewer->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
                        pEffectGnomesWhisper = dynamic_cast<EffectGnomesWhisper*>(
                            pViewer->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
                    }

                    if (!pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
                        pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) ||
                        (pEffectGnomesWhisper != NULL &&
                         pEffectGnomesWhisper->canSeeHide())) // || ( pEffectRevealer != NULL &&
                                                              // pEffectRevealer->canSeeHide( pCreature ) ) ))
                    {
                        if (pViewer->isVampire() || pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) ||
                            (pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(pCreature)) ||
                            (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeInvisibility())) {
                            pViewer->getPlayer()->sendPacket(&gcAddEffect);
                        } else {
                            pViewer->getPlayer()->sendPacket(&gcDO);
                        }
                        // Cannot see it, but only as far as invisibility goes.
                    } else {
                        // Could not see it before either, so there is nothing to do.
                    }
                } // if
            } // for
        } // for
    } // for

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Add a creature that was invisible and is now visible.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that was invisible
// bool        bForced   : did it become visible by force?
//////////////////////////////////////////////////////////////////////////////
void addVisibleCreature(Zone* pZone, Creature* pCreature, bool bForced)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    // Only a vampire or a monster can turn invisible.
    Assert(pCreature->isVampire() || pCreature->isMonster());

    // The flag must be on.
    Assert(pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY));

    ZoneCoord_t cx = pCreature->getX();
    ZoneCoord_t cy = pCreature->getY();

    Packet* pGCAddXXX = NULL;
    GCAddMonster gcAddMonster;
    GCAddVampire gcAddVampire;
    GCAddBurrowingCreature gcABC;

    Creature::CreatureClass CClass = pCreature->getCreatureClass();

    if (CClass == Creature::CREATURE_CLASS_MONSTER) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) // Actually impossible
        {
            gcABC.setObjectID(pMonster->getObjectID());
            gcABC.setName(pMonster->getName());
            gcABC.setX(cx);
            gcABC.setY(cy);

            pGCAddXXX = &gcABC;
        } else {
            // It is not held in the monster's EffectManager,
            // so build one temporarily and send it.
            // A monster stays Invisible indefinitely.
            EffectInfo* pEffectInfo = new EffectInfo;
            pEffectInfo->addListElement(Effect::EFFECT_CLASS_INVISIBILITY, 0xFFFF);

            // make packet
            gcAddMonster.setObjectID(pMonster->getObjectID());
            gcAddMonster.setMonsterType(pMonster->getMonsterType());
            gcAddMonster.setMonsterName(pMonster->getName()); // by sigi - -;
            gcAddMonster.setX(cx);
            gcAddMonster.setY(cy);
            gcAddMonster.setDir(pMonster->getDir());
            gcAddMonster.setEffectInfo(pEffectInfo);
            gcAddMonster.setCurrentHP(pMonster->getHP());
            gcAddMonster.setMaxHP(pMonster->getHP(ATTR_MAX));

            pGCAddXXX = &gcAddMonster;
        }
    } else if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) // Actually impossible
        {
            gcABC.setObjectID(pVampire->getObjectID());
            gcABC.setName(pVampire->getName());
            gcABC.setX(cx);
            gcABC.setY(cy);

            pGCAddXXX = &gcABC;
        } else {
            makeGCAddVampire(&gcAddVampire, pVampire);
            pGCAddXXX = &gcAddVampire;
        }
    } else {
        throw Error();
    }

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_INVISIBILITY);
    if (pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);
        pPlayer->sendPacket(&gcRemoveEffect);
    }

    //--------------------------------------------------------------------------------
    //
    // Enlarge the sight area by 1 on every side.
    // The ON_SIGHT area grows with the direction.
    //
    //--------------------------------------------------------------------------------
    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1),
                     endx = min(pZone->getWidth() - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(pZone->getHeight() - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            Tile& curTile = pZone->getTile(ix, iy);
            const forward_list<Object*>& objectList = curTile.getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();
            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                Assert(*itr != NULL);

                Creature* pViewer = dynamic_cast<Creature*>(*itr);

                // Take the Viewer's Revealer effect.

                // Take the Viewer's Observing Eye effect.
                EffectObservingEye* pEffectObservingEye = NULL;
                if (pViewer->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
                    pEffectObservingEye =
                        dynamic_cast<EffectObservingEye*>(pViewer->findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
                }

                // Take the Viewer's Gnome's Whisper effect.
                EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
                if (pViewer->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
                    pEffectGnomesWhisper =
                        dynamic_cast<EffectGnomesWhisper*>(pViewer->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
                }

                if (pViewer != pCreature && pViewer->isPC() && (pViewer->getVisionState(cx, cy) >= IN_SIGHT)) {
                    {
                        // Add this character only for those who could not
                        // see it before.
                        // Cannot see it, but only as far as invisibility goes.
                        if (!pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) &&
                            (pViewer->isSlayer() || pViewer->isOusters()) &&
                            !(pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(pCreature)) &&
                            !(pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeInvisibility())) {
                            pViewer->getPlayer()->sendPacket(pGCAddXXX);
                        }
                    }
                    // else
                    {
                        // Could not see it before either, so there is nothing to do.
                    }

                    // Either way, the fact that invisible is lifted must be sent.
                    pViewer->getPlayer()->sendPacket(&gcRemoveEffect);

                } // if

            } // for

        } // for

    } // for


    //--------------------------------------------
    // Force-delete the Effect from the effect manager.
    //--------------------------------------------
    if (bForced == true) {
        EffectManager* pEffectManager = pCreature->getEffectManager();
        Assert(pEffectManager);
        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_INVISIBILITY);
    }


    pCreature->removeFlag(Effect::EFFECT_CLASS_INVISIBILITY);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Add an invisible creature.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the invisible creature
// ZoneCoord_t cx        : the creature's original x coordinate
// ZoneCoord_t cy        : the creature's original y coordinate
//////////////////////////////////////////////////////////////////////////////
void addSnipingModeCreature(Zone* pZone, Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    // Only a slayer can use this skill.
    Assert(pCreature->isSlayer());

    ObjectID_t creatureID = pCreature->getObjectID();

    GCDeleteObject gcDO;
    gcDO.setObjectID(creatureID);

    pCreature->setFlag(Effect::EFFECT_CLASS_SNIPING_MODE);


    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(creatureID);
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_SNIPING_MODE);
    gcAddEffect.setDuration(0);

    //--------------------------------------------------------------------------------
    //
    // Enlarge the sight area by 1 on every side.
    // The ON_SIGHT area grows with the direction.
    //
    //--------------------------------------------------------------------------------
    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1),
                     endx = min(pZone->getWidth() - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(pZone->getHeight() - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            Tile& curTile = pZone->getTile(ix, iy);
            const forward_list<Object*>& objectList = curTile.getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();
            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                Assert(*itr != NULL);
                Creature* pViewer = dynamic_cast<Creature*>(*itr);

                if (pViewer != pCreature && pViewer->isPC() && (pViewer->getVisionState(cx, cy) >= IN_SIGHT)) {
                    // Take the Viewer's Revealer effect.
                    // Take the Viewer's Gnome's Whisper effect.
                    EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
                    if (pViewer->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
                        pEffectGnomesWhisper = dynamic_cast<EffectGnomesWhisper*>(
                            pViewer->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
                    }

                    if (!pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
                        pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) ||
                        (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeHide()))
                    //						|| ( pEffectRevealer != NULL && pEffectRevealer->canSeeHide( pCreature ) )
                    //))
                    {
                        if (pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) ||
                            (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeSniping()))
                        //							|| ( pEffectRevealer != NULL && pEffectRevealer->canSeeSniping(
                        // pCreature ) ) )
                        {
                            pViewer->getPlayer()->sendPacket(&gcAddEffect);
                        } else {
                            pViewer->getPlayer()->sendPacket(&gcDO);
                        }

                        // Cannot see it, but only as far as invisibility goes.
                    } else {
                        // Could not see it before either, so there is nothing to do.
                    }
                } // if
            } // for
        } // for
    } // for

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Add a creature that left sniping mode.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature that was in sniping mode
// bool        bForced   : did it leave sniping mode by force?
//////////////////////////////////////////////////////////////////////////////
void addUnSnipingModeCreature(Zone* pZone, Creature* pCreature, bool bForced)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pCreature != NULL);

    // Only a slayer can use sniping.
    Assert(pCreature->isSlayer());

    // The flag must be on.
    Assert(pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE));

    ZoneCoord_t cx = pCreature->getX();
    ZoneCoord_t cy = pCreature->getY();

    Packet* pGCAddXXX = NULL;
    GCAddSlayer gcAddSlayer;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    makeGCAddSlayer(&gcAddSlayer, pSlayer);
    pGCAddXXX = &gcAddSlayer;

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_SNIPING_MODE);

    if (pCreature->isPC()) {
        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer);
        pPlayer->sendPacket(&gcRemoveEffect);
    }

    //--------------------------------------------------------------------------------
    //
    // Enlarge the sight area by 1 on every side.
    // The ON_SIGHT area grows with the direction.
    //
    //--------------------------------------------------------------------------------
    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1),
                     endx = min(pZone->getWidth() - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(pZone->getHeight() - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            Tile& curTile = pZone->getTile(ix, iy);
            const forward_list<Object*>& objectList = curTile.getObjectList();

            forward_list<Object*>::const_iterator itr = objectList.begin();
            for (; itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                 itr++) {
                Assert(*itr != NULL);

                Creature* pViewer = dynamic_cast<Creature*>(*itr);

                // Take the Viewer's Revealer effect.
                // Take the Viewer's Gnome's Whisper effect.
                EffectGnomesWhisper* pEffectGnomesWhisper = NULL;
                if (pViewer->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
                    pEffectGnomesWhisper =
                        dynamic_cast<EffectGnomesWhisper*>(pViewer->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
                }

                if (pViewer != pCreature && pViewer->isPC() && (pViewer->getVisionState(cx, cy) >= IN_SIGHT)) {
                    if (!pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
                        pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN) ||
                        (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeHide()))
                    //						|| ( pEffectRevealer != NULL && pEffectRevealer->canSeeHide( pCreature ) )
                    //))
                    {
                        // Cannot see it, but only as far as invisibility goes.
                        if (!pViewer->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) ||
                            (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeSniping()))
                        //							&& !( pEffectRevealer != NULL && pEffectRevealer->canSeeSniping(
                        // pCreature ) ) )
                        {
                            pViewer->getPlayer()->sendPacket(pGCAddXXX);
                        }
                    } else {
                        // Could not see it before either, so there is nothing to do.
                    }

                    // Send that sniping mode is lifted.
                    pViewer->getPlayer()->sendPacket(&gcRemoveEffect);

                } // if

            } // for

        } // for

    } // for


    //--------------------------------------------
    // Force-delete the Effect from the effect manager.
    //--------------------------------------------
    if (bForced == true) {
        EffectManager* pEffectManager = pCreature->getEffectManager();
        Assert(pEffectManager);
        pEffectManager->deleteEffect(pCreature, Effect::EFFECT_CLASS_SNIPING_MODE);
    }

    pCreature->removeFlag(Effect::EFFECT_CLASS_SNIPING_MODE);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Add a mine to the zone.
//
// Zone*       pZone : pointer to the zone
// Mine*       pMine : pointer to the mine object
// ZoneCoord_t cx    : x coordinate to add the mine at
// ZoneCoord_t cy    : y coordinate to add the mine at
//////////////////////////////////////////////////////////////////////////////
void addInstalledMine(Zone* pZone, Mine* pMine, ZoneCoord_t cx, ZoneCoord_t cy)

{
    __BEGIN_TRY

    Assert(pZone != NULL);
    Assert(pMine != NULL);
    Assert(pMine->isFlag(Effect::EFFECT_CLASS_INSTALL));


    GCDeleteObject gcDO;
    gcDO.setObjectID(pMine->getObjectID());

    GCAddInstalledMineToZone gcAddMine;
    gcAddMine.setObjectID(pMine->getObjectID());
    gcAddMine.setX(cx);
    gcAddMine.setY(cy);
    gcAddMine.setItemClass(pMine->getItemClass());
    gcAddMine.setItemType(pMine->getItemType());
    gcAddMine.setOptionType(pMine->getOptionTypeList());
    gcAddMine.setDurability(pMine->getDurability());

    //--------------------------------------------------------------------------------
    //
    // Enlarge the sight area by 1 on every side.
    // The ON_SIGHT area grows with the direction.
    //
    //--------------------------------------------------------------------------------
    for (ZoneCoord_t ix = max(0, cx - maxViewportWidth - 1),
                     endx = min(pZone->getWidth() - 1, cx + maxViewportWidth + 1);
         ix <= endx; ix++) {
        for (ZoneCoord_t iy = max(0, cy - maxViewportUpperHeight - 1),
                         endy = min(pZone->getHeight() - 1, cy + maxViewportLowerHeight + 1);
             iy <= endy; iy++) {
            Tile& rTile2 = pZone->getTile(ix, iy);
            const forward_list<Object*>& objectList = rTile2.getObjectList();

            for (forward_list<Object*>::const_iterator itr = objectList.begin();
                 itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE; itr++) {
                Assert(*itr != NULL);

                Creature* pViewer = dynamic_cast<Creature*>(*itr);

                Assert(pViewer != NULL);

                if (pViewer->isPC() && (pViewer->getVisionState(cx, cy) >= IN_SIGHT)) {
                    Player* pPlayer = pViewer->getPlayer();
                    Assert(pPlayer);
                    pPlayer->sendPacket(&gcDO);

                    if (pViewer->isFlag(Effect::EFFECT_CLASS_REVEALER)) {
                        pPlayer->sendPacket(&gcAddMine);
                    }
                } // if
            } // for
        } // for
    } // for

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Check whether a given creature stepped on a mine.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature to check
// ZoneCoord_t X         : x coordinate to check
// ZoneCoord_t Y         : y coordinate to check
//////////////////////////////////////////////////////////////////////////////
bool checkMine(Zone* pZone, Creature* pCreature, ZoneCoord_t X, ZoneCoord_t Y)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    static const int serverID = de::kernelContext().config().getPropertyInt("ServerID");
    static const int worldID = de::kernelContext().config().getPropertyInt("WorldID");
    static bool bNonPK = de::serverContext().serverInfos().getGameServerInfo(1, serverID, worldID)->isNonPKServer();
    if (bNonPK && pCreature->isPC())
        return false;

    // A slayer cannot set one off.
    if (pCreature->isSlayer())
        return false;

    Assert(pZone != NULL);

    // No explosion in a safe zone.
    if (pZone->getZoneLevel(X, Y) & SAFE_ZONE)
        return false;

    Tile& rTile = pZone->getTile(X, Y);

    // No explosion when the tile holds no item either.
    if (!rTile.hasItem())
        return false;


    Item* pItem = rTile.getItem();

    // The mine does not explode if the item on the ground is not an
    // installed mine, or if the creature is not a walking creature.
    if (pItem->getItemClass() != Item::ITEM_CLASS_MINE)
        return false;
    if (pItem->isFlag(Effect::EFFECT_CLASS_INSTALL) == false)
        return false;
    if (pCreature->isWalking() == false)
        return false;

    GCMineExplosionOK1 _GCMineExplosionOK1;
    GCMineExplosionOK2 _GCMineExplosionOK2;

    list<Creature*> cList;

    Mine* pMine = dynamic_cast<Mine*>(pItem);
    Assert(pMine != NULL);

    Dir_t Dir = pMine->getDir();
    Damage_t Damage = pMine->getDamage();
    ItemType_t Type = pMine->getItemType();
    string InstallerName = pMine->getInstallerName();
    int PartyID = pMine->getInstallerPartyID();

    BYTE explodeType = Type; // Explosion shape

    // The mine exploded, so delete it unconditionally.
    pZone->deleteItem(pMine, X, Y);

    GCDeleteObject gcDO;
    gcDO.setObjectID(pMine->getObjectID());
    pZone->broadcastPacket(X, Y, &gcDO);

    SAFE_DELETE(pMine);


    int tileX, tileY;

    const int* xOffsetByEType = NULL;
    const int* yOffsetByEType = NULL;
    int tiles = 0;

    // Take the explosion offset mask for the mine type.
    getExplosionTypeXYOffset(explodeType, Dir, xOffsetByEType, yOffsetByEType, tiles);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    for (int tileI = 0; tileI < tiles; tileI++) {
        tileX = X + xOffsetByEType[tileI];
        tileY = Y + yOffsetByEType[tileI];

        // If the coordinate is inside the zone and not in a safe zone...
        if (rect.ptInRect(tileX, tileY) && !(pZone->getZoneLevel(tileX, tileY) & SAFE_ZONE)) {
            const Tile& tile = pZone->getTile(tileX, tileY);
            const forward_list<Object*>& oList = tile.getObjectList();

            // Examine every object on the tile.
            for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                // Check the condition.
                Object* pObject = *itr;
                if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    // Only things that take Damage are added to cList.
                    Creature* pTargetCreature = dynamic_cast<Creature*>(pObject);
                    if (pTargetCreature->isSlayer()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isVampire()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isOusters()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isMonster()) {
                        cList.push_back(pTargetCreature);
                    } else
                        continue;

                    ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                    _GCMineExplosionOK1.addCListElement(targetObjectID);
                    _GCMineExplosionOK2.addCListElement(targetObjectID);
                }
            }
        }
    }

    _GCMineExplosionOK1.setXYDir(X, Y, Dir);
    _GCMineExplosionOK1.setItemType(Type);

    _GCMineExplosionOK2.setXYDir(X, Y, Dir);
    _GCMineExplosionOK2.setItemType(Type);

    for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
        Creature* pTargetCreature = *itr;
        _GCMineExplosionOK1.clearList();

        if (pTargetCreature->isSlayer())
            Damage = max(1, Damage / 2);
        setDamage(pTargetCreature, Damage, NULL, 0, &_GCMineExplosionOK1);

        if (pTargetCreature->isPC()) {
            pTargetCreature->getPlayer()->sendPacket(&_GCMineExplosionOK1);
        } else if (pTargetCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);

            // Add the damage for the creature that installed the mine.
            // If the target is a monster and the attacker a player, the
            // damage-based precedence table must be updated.
            pMonster->addPrecedence(InstallerName, PartyID, Damage);
            pMonster->setLastHitCreatureClass(Creature::CREATURE_CLASS_SLAYER);
        }
    }

    pZone->broadcastPacket(X, Y, &_GCMineExplosionOK2, cList);

    return true;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Check whether a mine explodes in a chain.
//
// Zone*       pZone     : pointer to the zone
// Creature*   pCreature : the creature to check
// ZoneCoord_t X         : x coordinate to check
// ZoneCoord_t Y         : y coordinate to check
//////////////////////////////////////////////////////////////////////////////
bool checkMine(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    // No explosion in a safe zone.
    if (pZone->getZoneLevel(X, Y) & SAFE_ZONE)
        return false;

    Tile& rTile = pZone->getTile(X, Y);

    // No explosion when the tile holds no item either.
    if (rTile.hasItem() == false)
        return false;


    Item* pItem = rTile.getItem();

    // The mine only explodes when the item on the ground is an installed mine.
    if (pItem->getItemClass() != Item::ITEM_CLASS_MINE)
        return false;
    if (pItem->isFlag(Effect::EFFECT_CLASS_INSTALL) == false)
        return false;

    GCMineExplosionOK1 _GCMineExplosionOK1;
    GCMineExplosionOK2 _GCMineExplosionOK2;

    list<Creature*> cList;

    Mine* pMine = dynamic_cast<Mine*>(pItem);
    Assert(pMine != NULL);

    Dir_t Dir = pMine->getDir();
    Damage_t Damage = pMine->getDamage();
    ItemType_t Type = pMine->getItemType();
    string InstallerName = pMine->getInstallerName();
    int PartyID = pMine->getInstallerPartyID();

    BYTE explodeType = Type; // Explosion shape

    // The mine exploded, so delete it.
    pZone->deleteItem(pMine, X, Y);

    GCDeleteObject gcDO;
    gcDO.setObjectID(pMine->getObjectID());
    pZone->broadcastPacket(X, Y, &gcDO);

    SAFE_DELETE(pMine);


    int tileX, tileY;

    const int* xOffsetByEType = NULL;
    const int* yOffsetByEType = NULL;
    int tiles = 0;

    // Take the explosion offset mask for the mine type.
    getExplosionTypeXYOffset(explodeType, Dir, xOffsetByEType, yOffsetByEType, tiles);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    for (int tileI = 0; tileI < tiles; tileI++) {
        tileX = X + xOffsetByEType[tileI];
        tileY = Y + yOffsetByEType[tileI];

        // If the coordinate is inside the zone and not in a safe zone...
        if (rect.ptInRect(tileX, tileY) && !(pZone->getZoneLevel(tileX, tileY) & SAFE_ZONE)) {
            const Tile& tile = pZone->getTile(tileX, tileY);
            const forward_list<Object*>& oList = tile.getObjectList();

            // Examine every object on the tile.
            for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                // Check the condition.
                Object* pObject = *itr;
                if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    // Only things that take Damage are added to cList.
                    Creature* pTargetCreature = dynamic_cast<Creature*>(pObject);
                    if (pTargetCreature->isSlayer()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isVampire()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isOusters()) {
                        cList.push_back(pTargetCreature);
                    } else if (pTargetCreature->isMonster()) {
                        cList.push_back(pTargetCreature);
                    } else
                        continue;

                    ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                    _GCMineExplosionOK1.addCListElement(targetObjectID);
                    _GCMineExplosionOK2.addCListElement(targetObjectID);
                }
            }
        }
    }

    _GCMineExplosionOK1.setXYDir(X, Y, Dir);
    _GCMineExplosionOK1.setItemType(Type);

    _GCMineExplosionOK2.setXYDir(X, Y, Dir);
    _GCMineExplosionOK2.setItemType(Type);

    for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
        Creature* pTargetCreature = *itr;
        _GCMineExplosionOK1.clearList();

        if (pTargetCreature->isSlayer())
            Damage = max(1, Damage / 2);
        setDamage(pTargetCreature, Damage, NULL, 0, &_GCMineExplosionOK1);

        if (pTargetCreature->isPC()) {
            pTargetCreature->getPlayer()->sendPacket(&_GCMineExplosionOK1);
        } else if (pTargetCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);

            // Add the damage for the creature that installed the mine.
            // If the target is a monster and the attacker a player, the
            // damage-based precedence table must be updated.
            pMonster->addPrecedence(InstallerName, PartyID, Damage);
            pMonster->setLastHitCreatureClass(Creature::CREATURE_CLASS_SLAYER);
        }
    }

    pZone->broadcastPacket(X, Y, &_GCMineExplosionOK2, cList);

    return true;

    __END_CATCH
}

bool checkTrap(Zone* pZone, Creature* pCreature) {
    if (!isValidZoneCoord(pZone, pCreature->getX(), pCreature->getY()))
        return false;

    Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());
    Effect* pEffect = rTile.getEffect(Effect::EFFECT_CLASS_TRAP_INSTALLED);
    if (pEffect == NULL)
        return false;

    int ratio = 0;

    if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        ratio = 100 - (pMonster->getLevel() / 10);
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        ratio = 100 - (pVampire->getINT() / 8);
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        ratio = 100 - (pOusters->getINT() / 8);
    } else
        return false;

    if (rand() % 100 > ratio)
        return false;

    EffectTrapInstalled* pTrap = dynamic_cast<EffectTrapInstalled*>(pEffect);
    pTrap->affect(pCreature);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Move a given creature to another zone.
//
// Creature*   pCreature    : the creature to move
// ZoneID_t    TargetZoneID : the zone ID to move to
// ZoneCoord_t TargetX      : the X coordinate in the target zone
// ZoneCoord_t TargetY      : the Y coordinate in the target zone
// bool        bSendMoveOK  : whether GCMoveOK is sent
//////////////////////////////////////////////////////////////////////////////
void transportCreature(Creature* pCreature, ZoneID_t TargetZoneID, ZoneCoord_t TX, ZoneCoord_t TY, bool bSendMoveOK)

{
    __BEGIN_TRY

    ResurrectLocationManager& resurrectLocations = de::gameContext().resurrectLocations();

    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    Zone* pZone = pCreature->getZone();

    // transportCreature is only allowed while the state is GPS_NORMAL.
    // Other cases are ignored.
    // by sigi. 2002.12.10

    if (pGamePlayer->getPlayerStatus() != GPS_NORMAL) {
        filelog("transportCreatureError.log",
                "PlayerStatus not GPS_NORMAL: %d, Current[%d, (%d,%d)] --> Target[%d, (%d,%d)]",
                (int)pGamePlayer->getPlayerStatus(), (int)pZone->getZoneID(), (int)pCreature->getX(),
                (int)pCreature->getY(), (int)TargetZoneID, (int)TX, (int)TY);

        return;
    }

    cout << "ZoneUtil.cpp step 1" << endl;


    Assert(pGamePlayer != NULL);
    Assert(pZone != NULL);

    if (bSendMoveOK) {
        cout << "ZoneUtil.cpp step 2" << endl;
        // Send GCMoveOK for the sake of the dumb client.
        GCMoveOK gcMoveOK(pCreature->getX(), pCreature->getY(), pCreature->getDir());
        pGamePlayer->sendPacket(&gcMoveOK);
    }


    ZoneInfoManager& zoneInfos = de::gameContext().zoneInfos();

    //  Block warp/zone movement according to ZoneInfo's OpenLevel.
    //  add by inthesky 2004.07.26

    ZoneInfo* pZoneInfo = zoneInfos.getZoneInfo(TargetZoneID);

    // add by Sonic 2006.10.21

    if (TargetZoneID == 1013) // Zone that costs a Moon Card to enter
    {
        cout << "ZoneUtil.cpp step New1013" << endl;
        if (pZoneInfo->isNoPortalZone()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
            CoordInven_t InvenX = 0;
            CoordInven_t InvenY = 0;
            ItemType_t fitItem = 3; // Moon Card item type
            Item* pItem = pPC->getInventory()->findItem(Item::ITEM_CLASS_MOON_CARD, fitItem, InvenX, InvenY);
            GCSystemMessage gcSystemMessage1;
            if (pItem == NULL) {
                gcSystemMessage1.setMessage("You need a silver coin to enter this dungeon!");
                pGamePlayer->sendPacket(&gcSystemMessage1);
                return;
            }
            ItemNum_t OldNum = pItem->getNum();
            if (OldNum == 1) {
                if (pItem->isTimeLimitItem() == true) {
                } else {
                    cout << " User: = " << pCreature->getName() << endl;
                    cout << " pItem->Hour = " << (int)pItem->getHour() << endl;
                    cout << " pItem->isTimeLimitItem = " << (int)pItem->isTimeLimitItem() << endl;
                    cout << " pItem->Num = " << (int)pItem->getNum() << endl;
                    pPC->getInventory()->deleteItem(pItem->getObjectID());
                    pItem->destroy();
                    SAFE_DELETE(pItem);
                }
            } else {
                OldNum--;
                pItem->setNum((pItem->getNum() - 1));
                pItem->save(pPC->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
            }
        }
    }
    // end by sonic


    cout << "ZoneUtil.cpp step 3" << endl;

    bool bNoMoney = false;

    try {
        ZoneInfo* pZoneInfo = zoneInfos.getZoneInfo(TargetZoneID);

        // A pay zone while the account is not paying.
        if (pZoneInfo != NULL && (pZoneInfo->isPayPlay() || pZoneInfo->isPremiumZone()) &&
            !pGamePlayer->isPayPlaying() &&
            !(de::gameContext().warSystem().hasActiveRaceWar() && pZoneInfo->isHolyLand())) {
            cout << "ZoneUtil.cpp step 4" << endl;

            bool bEnterZone = true;

            string connectIP = pGamePlayer->getSocket()->getHost();

            // Is the pay service available?
            if (pGamePlayer->loginPayPlay(connectIP, pGamePlayer->getID())) {
                cout << "ZoneUtil.cpp step 5" << endl;

                sendPayInfo(pGamePlayer);

                // Find the zone.
                Zone* pZone = getZoneByZoneID(TargetZoneID);
                Assert(pZone != NULL);

                // Can the master lair be entered?
                // Can a PK zone be entered?
                bEnterZone = enterMasterLair(pZone, pCreature);
            } else if (pZoneInfo->isPayPlay() &&
                       !pGamePlayer->isFamilyFreePass()) // A family free pass may enter a pay zone.
            {
                cout << "ZoneUtil.cpp step 6" << endl;

                bEnterZone = false;
            }

            if (!bEnterZone) {
                cout << "ZoneUtil.cpp step 7" << endl;

                // The current zone cannot be entered:
                // the pay service is unavailable,
                // or it is a master lair.
                // slayer : go to the resurrection point in south-east Eslan.
                // vampire : go to the resurrection point in south-east Limbo.
                ZONE_COORD zoneCoord;
                bool bFindPos = false;

                if (pCreature->isSlayer())
                    bFindPos = resurrectLocations.getSlayerPosition(13, zoneCoord);
                else if (pCreature->isVampire())
                    bFindPos = resurrectLocations.getVampirePosition(23, zoneCoord);
                else if (pCreature->isOusters())
                    bFindPos = resurrectLocations.getOustersPosition(1311, zoneCoord);

                if (bFindPos) {
                    TargetZoneID = zoneCoord.id;
                    TX = zoneCoord.x;
                    TY = zoneCoord.y;

                    bNoMoney = true;
                } else {
                    // Emergency.
                    filelog("zoneUtilError.txt", "[ZoneUtil::transportCreature] ResurrectInfo is not esta..");
                    throw Error("Critical Error : ResurrectInfo is not established!1");
                }
            }
        }
    } catch (NoSuchElementException& no) {
        filelog("zoneUtilError.txt", "[ZoneUtil::transportCreature] %s", no.toString().c_str());
        throw Error(no.toString());
    }


    // First delete the PC from the previous zone and move the player from ZPM to IPM.
    try {
        // If a slayer who has not paid is riding a motorcycle,
        // remove the motorcycle.
        if (bNoMoney && pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            if (pSlayer->hasRideMotorcycle()) {
                pSlayer->getOffMotorcycle();

                GCGetOffMotorCycle _GCGetOffMotorCycle;
                _GCGetOffMotorCycle.setObjectID(pSlayer->getObjectID());
                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCGetOffMotorCycle);
            }
        }

        cout << "ZoneUtil.cpp step 8" << endl;
        // Save the creature's information.
        pCreature->save();

        ZoneInfo* pZoneInfo = zoneInfos.getZoneInfo(TargetZoneID);
        Assert(pZoneInfo != NULL);

        // Drop the Blood Bible when leaving the holy land with it.
        // It is dropped when carried out into the castle dungeon too.
        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE)) {
            if (pZone->isHolyLand()) {
                if (!pZoneInfo->isHolyLand() ||
                    (!pZoneInfo->isCastle() &&
                     de::gameContext().castleInfos().isSameCastleZone(pZone->getZoneID(), TargetZoneID)))
                    dropRelicToZone(pCreature);
            }
        }

        // Drop the castle symbol when leaving the castle with it.
        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL)) {
            if ((pZone->isHolyLand() && !pZoneInfo->isHolyLand()) ||
                !de::gameContext().castleInfos().isSameCastleZone(pCreature->getZone()->getZoneID(), TargetZoneID)
                // The castle cannot be entered; the castle symbol lives on the castle basement map.
                || pZoneInfo->isCastle()) {
                dropRelicToZone(pCreature);
            }
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_FLAG)) {
            if (de::gameContext().flags().isFlagAllowedZone(pZone->getZoneID()) &&
                !de::gameContext().flags().isFlagAllowedZone(pZoneInfo->getZoneID())) {
                dropFlagToZone(pCreature);
            }
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))
            ;
        dropSweeperToZone(pCreature);

        // Call initAllStat when moving out of the holy land or into it.
        if (pZone->isHolyLand() != pZoneInfo->isHolyLand()) {
            pCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        }

        if (de::gameContext().levelWarZones().isCreatureBonusZone(pCreature, pZone->getZoneID()) !=
            de::gameContext().levelWarZones().isCreatureBonusZone(pCreature, TargetZoneID)) {
            pCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        }

        if (pZone->isLevelWarZone() != pZoneInfo->isLevelWarZone()) {
            pCreature->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        }

        // Now delete the PC from the zone.
        //
        // *CAUTION*
        // pCreature's coordinates must match the tile it actually stands on.
        // So the coordinates must be set correctly before calling this method.
        pZone->deleteCreature(pCreature, pCreature->getX(), pCreature->getY());

        // Delete the player from the zone group's ZPM.
        pZone->getZoneGroup()->getZonePlayerManager()->deletePlayer(pGamePlayer->getSocket()->getSOCKET());

        // The creature's new coordinates are the portal's destination.
        cout << "ZoneUtil.cpp step 9" << endl;

        // Move the player to the IPM.
        pZone->getZoneGroup()->getZonePlayerManager()->pushOutPlayer(pGamePlayer);
    } catch (NoSuchElementException& nsee) {
        filelog("zoneUtilError.txt", "[ZoneUtil::transportCreature2] %s", nsee.toString().c_str());
        throw Error(nsee.toString());
    }

    // Assign the zone to the creature so that it can be given an OID.
    // Set the zone to move to.
    Zone* pNewZone = getZoneByZoneID(TargetZoneID);
    Assert(pNewZone != NULL);

    pCreature->setNewZone(pNewZone);
    pCreature->setNewXY(TX, TY);

    // Allocate OIDs for the creature itself and its owned items.

    // Done in ZonePlayerManager's heartbeat.


    // Moving into Adam's holy land from elsewhere, or
    // out of Adam's holy land to somewhere else.
    if ((!pZone->isHolyLand() && pNewZone->isHolyLand()) || (pZone->isHolyLand() && !pNewZone->isHolyLand())) {
        sendHolyLandWarpEffect(pCreature);
        cout << "ZoneUtil.cpp step 10" << endl;
    }

    // change player status
    pGamePlayer->setPlayerStatus(GPS_WAITING_FOR_CG_READY);

    cout << "ZoneUtil.cpp step 11" << endl;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Find the zone with a given zone ID and return a pointer to it.
// ZoneID_t ZID : the zone ID to look for
//////////////////////////////////////////////////////////////////////////////
Zone* getZoneByZoneID(ZoneID_t ZID)

{
    __BEGIN_TRY

    ZoneInfo* pZoneInfo = NULL;
    try {
        pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(ZID);
    } catch (NoSuchElementException&) {
        StringStream msg;
        msg << "getZoneByZoneID() : No Such ZoneInfo [" << (int)ZID << "]";
        throw Error(msg.toString());
    }

    ZoneGroup* pZoneGroup = NULL;
    try {
        pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(pZoneInfo->getZoneGroupID());
    } catch (NoSuchElementException&) {
        // There is only one server for now, so bail out.
        throw Error("getZoneByZoneID() : No Such ZoneGroup");
    }

    Zone* pZone = pZoneGroup->getZone(ZID);
    Assert(pZone != NULL);

    return pZone;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Game master command: add monsters of a given type to the zone.
//////////////////////////////////////////////////////////////////////////////
void addMonstersToZone(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, SpriteType_t SType, MonsterType_t MType, int num,
                       const SUMMON_INFO& summonInfo, list<Monster*>* pSummonedMonsters)

{
    __BEGIN_TRY

    try {
        MonsterManager* pMonsterManager = pZone->getMonsterManager();
        Assert(pMonsterManager != NULL);


        if (SType != 0) {
            const vector<MonsterType_t>& monsterTypes = de::gameContext().monsterInfos().getMonsterTypeBySprite(SType);

            if (!monsterTypes.empty()) {
                // Create num monsters.
                for (int i = 0; i < num; i++) {
                    MonsterType_t monsterType = monsterTypes[rand() % monsterTypes.size()];

                    pMonsterManager->addMonsters(x, y, monsterType, 1, summonInfo, pSummonedMonsters);
                }
            }
        } else if (MType != 0) {
            pMonsterManager->addMonsters(x, y, MType, num, summonInfo, pSummonedMonsters);
        }
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Add monsters of a given type to the zone.
//////////////////////////////////////////////////////////////////////////////
void addMonstersToZone(Zone* pZone, const SUMMON_INFO2& summonInfo, list<Monster*>* pSummonedMonsters)

{
    __BEGIN_TRY

    try {
        MonsterCollection* pCollection = summonInfo.pMonsters;

        if (pCollection == NULL)
            return;

        MonsterManager* pMonsterManager = pZone->getMonsterManager();
        Assert(pMonsterManager != NULL);


        list<MonsterCollectionInfo>& Infos = pCollection->Infos;
        list<MonsterCollectionInfo>::const_iterator itr;
        for (itr = Infos.begin(); itr != Infos.end(); itr++) {
            const MonsterCollectionInfo& monsterInfo = *itr;

            if (monsterInfo.SpriteType != 0) {
                const vector<MonsterType_t>& monsterTypes =
                    de::gameContext().monsterInfos().getMonsterTypeBySprite(monsterInfo.SpriteType);

                if (!monsterTypes.empty()) {
                    // Create Num monsters.
                    for (int i = 0; i < monsterInfo.Num; i++) {
                        MonsterType_t monsterType = monsterTypes[rand() % monsterTypes.size()];

                        pMonsterManager->addMonsters(summonInfo.X, summonInfo.Y, monsterType, 1, summonInfo,
                                                     pSummonedMonsters);
                    }
                }
            } else if (monsterInfo.MonsterType != 0) {
                pMonsterManager->addMonsters(summonInfo.X, summonInfo.Y, monsterInfo.MonsterType, monsterInfo.Num,
                                             summonInfo, pSummonedMonsters);
            }
        }
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Check whether a given creature is currently inside a safe zone.
// Used when trading.
//////////////////////////////////////////////////////////////////////////////
bool isInSafeZone(Creature* pCreature) {
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    ZoneLevel_t ZoneLevel = pZone->getZoneLevel(pCreature->getX(), pCreature->getY());

    if (pCreature->isSlayer() && (ZoneLevel & SLAYER_SAFE_ZONE))
        return true;
    if (pCreature->isVampire() && (ZoneLevel & VAMPIRE_SAFE_ZONE))
        return true;
    if (pCreature->isOusters() && (ZoneLevel & OUSTERS_SAFE_ZONE))
        return true;
    if (ZoneLevel & COMPLETE_SAFE_ZONE)
        return true;

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Check whether the coordinates are inside the zone's bounds.
//////////////////////////////////////////////////////////////////////////////
bool isValidZoneCoord(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, int offset) {
    Assert(pZone != NULL);

    VSRect rect;
    rect.left = 0 + offset;
    rect.top = 0 + offset;
    rect.right = pZone->getWidth() - offset - 1;
    rect.bottom = pZone->getHeight() - offset - 1;

    if (rect.ptInRect(x, y))
        return true;

    return false;
}

bool enterMasterLair(Zone* pZone, Creature* pCreature)

{
    __BEGIN_TRY

    if (pZone == NULL || pCreature == NULL)
        return false;

    // Nothing to check when it is not a master lair.
    if (!pZone->isMasterLair()) {
        return true;
    }

    MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
    Assert(pMasterLairManager != NULL);

    if (pMasterLairManager->enterCreature(pCreature)) {
        // Entry allowed.
        return true;
    }

    __END_CATCH

    return false;
}

void getNewbieTransportZoneInfo(Slayer* pSlayer, ZONE_COORD& zoneInfo) {
    // Pick the newbie transport destination from the slayer's highest skill domain.
    zoneInfo.x = 30;
    zoneInfo.y = 42;

    switch (pSlayer->getHighestSkillDomain()) {
    case SKILL_DOMAIN_HEAL:
    case SKILL_DOMAIN_ENCHANT:
        zoneInfo.id = 2010;
        break;

    case SKILL_DOMAIN_GUN:
        zoneInfo.id = 2000;
        break;

    default:
        zoneInfo.id = 2020;
        break;
    }
}


void checkNewbieTransportToGuild(Slayer* pSlayer) {
    StringPool& strings = de::gameContext().strings();

    try {
        if (pSlayer->isPLAYER() && de::gameContext().variables().isNewbieTransportToGuild()) {
            // If the attribute sum is 40 and the zone is the field headquarters, send the player elsewhere.
            ZONE_COORD transportZone;

            getNewbieTransportZoneInfo(pSlayer, transportZone);

            ZoneID_t zoneID = pSlayer->getZone()->getZoneID();
            if (zoneID == 2101) // || zoneID==2102)
            {
                Attr_t BasicSUM =
                    pSlayer->getSTR(ATTR_BASIC) + pSlayer->getDEX(ATTR_BASIC) + pSlayer->getINT(ATTR_BASIC);

                if (BasicSUM >= 39) {
                    GCSystemMessage gcSystemMessage;
                    gcSystemMessage.setMessage(strings.getString(STRID_NEWBIE_TRANSPORT_TO_GUILD));
                    pSlayer->getPlayer()->sendPacket(&gcSystemMessage);
                }

                // else
                if (BasicSUM >= 40) {
                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);

                    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
                    Event* pEvent = pGamePlayer->getEvent(Event::EVENT_CLASS_TRANSPORT);

                    if (pEvent == NULL) {
                        ZoneID_t ZoneID;
                        ZoneCoord_t ZoneX = 30, ZoneY = 42;
                        string ZoneName;

                        switch (pSlayer->getHighestSkillDomain()) {
                        case SKILL_DOMAIN_HEAL:
                        case SKILL_DOMAIN_ENCHANT:
                            ZoneID = 2010;
                            ZoneName = strings.getString(STRID_CLERIC_GUILD);
                            break;

                        case SKILL_DOMAIN_GUN:
                            ZoneID = 2000;
                            ZoneName = strings.getString(STRID_SOLDIER_GUILD);
                            break;

                        default:
                            ZoneID = 2020;
                            ZoneName = strings.getString(STRID_KNIGHT_GUILD);
                            break;
                        }


                        Turn_t deadline = 600;                   // 1 minute later
                        int timePenalty = (BasicSUM - 40) * 100; // 10 seconds per attribute point
                        deadline -= min(500, timePenalty);


                        Player* pPlayer = pSlayer->getPlayer();
                        Assert(pPlayer != NULL);

                        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);


                        EventTransport* pEventTransport = new EventTransport(pGamePlayer);

                        pEventTransport->setDeadline(deadline);
                        pEventTransport->setTargetZone(ZoneID, ZoneX, ZoneY);
                        pEventTransport->setZoneName(ZoneName);

                        // Tell the player where they will be moved to and in how many seconds.
                        pEventTransport->sendMessage();

                        pGamePlayer->addEvent(pEventTransport);
                    } else {
                        EventTransport* pEventTransport = dynamic_cast<EventTransport*>(pEvent);
                        pEventTransport->sendMessage();
                    }
                }
            }
        }
    } catch (Throwable& t) {
        filelog("newbieTransportBUG.log", "%s", t.toString().c_str());
    }
}

// Add a Corpse to the zone.
bool addCorpseToZone(Corpse* pCorpse, Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy)

{
    __BEGIN_TRY

    Assert(pCorpse != NULL);
    Assert(pZone != NULL);

    // Put the corpse into the zone as an item.
    TPOINT pt = pZone->addItem(pCorpse, cx, cy);
    if (pt.x == -1) {
        SAFE_DELETE(pCorpse);
        return false;
    }

    pCorpse->setX(pt.x);
    pCorpse->setY(pt.y);
    pCorpse->setZone(pZone);

    __END_CATCH

    return true;
}

// Check whether a corpse of a given monster type is inside the range.
// true if there is one, false otherwise.
bool checkCorpse(Zone* pZone, MonsterType_t MType, ZoneCoord_t x1, ZoneCoord_t y1, ZoneCoord_t x2, ZoneCoord_t y2)

{
    __BEGIN_TRY

    x1 = max(0, (int)x1);
    y1 = max(0, (int)y1);
    x2 = min(pZone->getWidth() - 1, (int)x2);
    y2 = min(pZone->getHeight() - 1, (int)y2);

    if (!isValidZoneCoord(pZone, x1, y1) || !isValidZoneCoord(pZone, x2, y2)) {
        return false;
    }

    ZoneCoord_t ix, iy;

    for (ix = x1; ix <= x2; ix++) {
        for (iy = y1; iy <= y2; iy++) {
            Tile& curTile = pZone->getTile(ix, iy);
            Item* pItem = curTile.getItem();

            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_CORPSE &&
                pItem->getItemType() == MONSTER_CORPSE) {
                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                if (pMonsterCorpse->getMonsterType() == MType) {
                    return true;
                }
            }
        }
    }

    return false;

    __END_CATCH
}

// A ZoneIDList of the zones belonging to a castle is needed to send a message to only some Zones.
void makeZoneIDList(const string& zoneIDs, list<ZoneID_t>& zoneIDList)

{
    __BEGIN_TRY

    size_t a = 0, b = 0;

    //////////////////////////////////////////////
    // 12345,67890,
    // a    ba    b
    //////////////////////////////////////////////
    zoneIDList.clear();
    if (zoneIDs.size() <= 1)
        return;

    do {
        b = zoneIDs.find_first_of(',', a);

        string zoneID = trim(zoneIDs.substr(a, b - a));

        // Plain atoi is probably fine here.
        zoneIDList.push_back(atoi(zoneID.c_str()));

        a = b + 1;

    } while (b != string::npos && b < zoneIDs.size() - 1);

    __END_CATCH
}

uint getZoneTimeband(Zone* pZone) {
    if (pZone == NULL) {
        return de::gameContext().worldTime().getTimeband();
    }

    return pZone->getTimeband();
}

bool createBulletinBoard(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y, MonsterType_t type, const string& msg,
                         const VSDateTime& timeLimit) {
    __BEGIN_TRY

    if (pZone->isMasterLair() || checkCorpse(pZone, type, X - 2, Y - 2, X + 2, Y + 2))
        return false;

    MonsterCorpse* pCorpse = new MonsterCorpse(type, msg, 2);
    Assert(pCorpse != NULL);

    pZone->registerObject(pCorpse);

    int delayTime = VSDateTime::currentDateTime().secsTo(timeLimit);
    TPOINT pt = pZone->addItem(pCorpse, X, Y, true, delayTime * 10);

    if (pt.x == -1) {
        SAFE_DELETE(pCorpse);
        return false;
    }

    string dbmsg = correctString(msg);
    uint affectedRows = defaultBulletinBoardRepository().insert(de::kernelContext().config().getPropertyInt("ServerID"),
                                                                pZone->getZoneID(), pt.x, pt.y, dbmsg, (uint)type,
                                                                timeLimit.toDateTime());

    if (affectedRows == 0) {
        filelog("BulletinBoard.log", "Failed to save to the DB. : %u, %u, %u, [%u:%s]", pZone->getZoneID(), pt.x, pt.y,
                type, msg.c_str());
    }

    return true;

    __END_CATCH
}

void loadBulletinBoard(Zone* pZone) {
    __BEGIN_TRY

    VSDateTime currentDateTime = VSDateTime::currentDateTime();

    vector<BulletinBoardRow> rows = defaultBulletinBoardRepository().loadForZone(
        de::kernelContext().config().getPropertyInt("ServerID"), pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        uint ID = rows[r].id;
        ZoneCoord_t X = rows[r].x;
        ZoneCoord_t Y = rows[r].y;
        string msg = rows[r].message;
        MonsterType_t type = rows[r].type;
        VSDateTime timeLimit(rows[r].timeLimit);

        if (timeLimit < currentDateTime) {
            cout << "Bulletin board post expired, deleting it: " << ID << " : [" << X << "," << Y << "] " << msg << " ["
                 << type << "] " << endl;
            defaultBulletinBoardRepository().remove(ID);
            continue;
        }

        int delayTime = currentDateTime.secsTo(timeLimit);

        MonsterCorpse* pCorpse = new MonsterCorpse(type, msg, 2);
        Assert(pCorpse != NULL);

        pZone->registerObject(pCorpse);

        TPOINT pt = pZone->addItem(pCorpse, X, Y, true, delayTime * 10);

        if (pt.x == -1) {
            filelog("BulletinBoard.log", "Read from the DB but could not be added to the zone. : %u, %u, %u, [%u:%s]",
                    pZone->getZoneID(), X, Y, type, msg.c_str());
        }
    }

    __END_CATCH
}

void forbidDarkness(Zone* pZone, ZoneCoord_t tX, ZoneCoord_t tY, int range) {
    for (int ti = -range; ti <= range; ++ti)
        for (int tj = -range; tj <= range; ++tj) {
            ZoneCoord_t X = tX + ti;
            ZoneCoord_t Y = tY + tj;

            if (!isValidZoneCoord(pZone, X, Y))
                continue;

            Tile& rTile = pZone->getTile(X, Y);
            if (!rTile.canAddEffect() || rTile.getEffect(Effect::EFFECT_CLASS_DARKNESS_FORBIDDEN) != NULL)
                continue;

            EffectDarknessForbidden* pEffect = new EffectDarknessForbidden(pZone, X, Y);
            pZone->registerObject(pEffect);
            rTile.addEffect(pEffect);
        }
}
