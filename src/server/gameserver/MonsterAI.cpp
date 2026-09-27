//////////////////////////////////////////////////////////////////////////////
// Filename    : MonsterAI.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MonsterAI.h"

#include "Directive.h"
#include "Effect.h"
#include "EffectManager.h"
#include "GCSay.h"
#include "GameContext.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterInfo.h"
#include "MonsterManager.h"
#include "Profile.h"
#include "SkillInfo.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Tile.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "skill/SkillHandlerManager.h"

#define ABS(X) (((X) >= 0) ? X : -X)

// #define __PROFILE_MONSTER__

#ifdef __PROFILE_MONSTER__
#define __BEGIN_PROFILE_MONSTER(name) beginProfileEx(name);
#define __END_PROFILE_MONSTER(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_MONSTER(name) ((void)0);
#define __END_PROFILE_MONSTER(name) ((void)0);
#endif

TPOINT getSafeTile(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y) {
    TPOINT ptAcid;
    ptAcid = findSuitablePositionForEffect(pZone, x, y, Effect::EFFECT_CLASS_ACID_SWAMP);

    return ptAcid;
}

int getStrongValue(Creature* pCreature) {
    HP_t currentHP = 0;
    HP_t maxHP = 0;
    int strongValue = 0;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        currentHP = pSlayer->getHP(ATTR_CURRENT);
        maxHP = pSlayer->getHP(ATTR_MAX);

        strongValue = pSlayer->getSTR() + pSlayer->getDEX() + pSlayer->getINT() + pSlayer->getProtection() +
                      pSlayer->getDefense();
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        currentHP = pVampire->getHP(ATTR_CURRENT);
        maxHP = pVampire->getHP(ATTR_MAX);

        strongValue = pVampire->getSTR() + pVampire->getDEX() + pVampire->getINT() + pVampire->getProtection() +
                      pVampire->getDefense();
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        currentHP = pOusters->getHP(ATTR_CURRENT);
        maxHP = pOusters->getHP(ATTR_MAX);

        strongValue = pOusters->getSTR() + pOusters->getDEX() + pOusters->getINT() + pOusters->getProtection() +
                      pOusters->getDefense();
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        currentHP = pMonster->getHP(ATTR_CURRENT);
        maxHP = pMonster->getHP(ATTR_MAX);

        strongValue = pMonster->getSTR() + pMonster->getDEX() + pMonster->getINT() + pMonster->getProtection() +
                      pMonster->getDefense();
    } else
        return 0;

    // pCreature HP is 120 or above
    if (currentHP >= 120) {
        strongValue += 200;
    }

    // If the HP percentage is 60% or above
    if (currentHP * 100 >= maxHP * 60) {
        strongValue += 200;
    }

    return strongValue;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
TPOINT randPos(int ox, int oy, int threshold, Zone* pZone)

{
    int tx = ox + (rand() % (threshold * 2)) - threshold;
    int ty = oy + (rand() % (threshold * 2)) - threshold;

    if (tx < 0)
        tx = 0;
    else if (tx >= pZone->getWidth())
        tx = pZone->getWidth() - 1;

    if (ty < 0)
        ty = 0;
    else if (ty >= pZone->getHeight())
        ty = pZone->getHeight() - 1;

    TPOINT pt;
    pt.x = tx;
    pt.y = ty;

    return pt;
}


//////////////////////////////////////////////////////////////////////////////
// Constructor / destructor
//////////////////////////////////////////////////////////////////////////////

MonsterAI::MonsterAI(Monster* pMonster, uint type)

{
    __BEGIN_TRY

    Assert(pMonster != NULL); // by sigi

    m_pBody = pMonster;
    m_AIType = type;
    m_Panic = 5;
    m_PanicMax = 5;
    m_Courage = 20;
    m_CourageMax = 20;
    m_bDamaged = false;
    m_LastAction = LAST_ACTION_NONE;

    m_MoveRule = MOVE_RULE_NORMAL;
    m_BlockedDir = DIR_NONE;
    m_WallCount = 0;

    m_pDirectiveSet = de::gameContext().directiveSets().getDirectiveSet(type);

    m_pCondChecker[DIRECTIVE_COND_ENEMY_RANGE_MELEE] = checkEnemyRangeMelee;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_RANGE_MISSILE] = checkEnemyRangeMissile;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_RANGE_CLOSE] = checkEnemyRangeClose;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_RANGE_OUT_OF_SIGHT] = checkEnemyRangeOutOfSight;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_DYING] = checkEnemyDying;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_BLOOD_DRAINED] = checkEnemyNotBloodDrained;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_GREEN_POISONED] = checkEnemyNotGreenPoisoned;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_YELLOW_POISONED] = checkEnemyNotYellowPoisoned;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_DARKBLUE_POISONED] = checkEnemyNotDarkbluePoisoned;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_GREEN_STALKERED] = checkEnemyNotGreenStalkered;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_PARALYZED] = checkEnemyNotParalyzed;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_DOOMED] = checkEnemyNotDoomed;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_BLINDED] = checkEnemyNotBlinded;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_IN_DARKNESS] = checkEnemyNotInDarkness;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_SEDUCTION] = checkEnemyNotSeduction;
    m_pCondChecker[DIRECTIVE_COND_IM_OK] = checkImOK;
    m_pCondChecker[DIRECTIVE_COND_IM_DYING] = checkImDying;
    m_pCondChecker[DIRECTIVE_COND_IM_DAMAGED] = checkImDamaged;
    m_pCondChecker[DIRECTIVE_COND_IM_HIDING] = checkImHiding;
    m_pCondChecker[DIRECTIVE_COND_IM_WOLF] = checkImWolf;
    m_pCondChecker[DIRECTIVE_COND_IM_BAT] = checkImBat;
    m_pCondChecker[DIRECTIVE_COND_IM_INVISIBLE] = checkImInvisible;
    m_pCondChecker[DIRECTIVE_COND_IM_WALKING_WALL] = checkImWalkingWall;
    m_pCondChecker[DIRECTIVE_COND_TIMING_BLOOD_DRAIN] = checkTimingBloodDrain;
    m_pCondChecker[DIRECTIVE_COND_MASTER_SUMMON_TIMING] = checkMasterSummonTiming;
    m_pCondChecker[DIRECTIVE_COND_MASTER_NOT_READY] = checkMasterNotReady;
    m_pCondChecker[DIRECTIVE_COND_IM_IN_BAD_POSITION] = checkImInBadPosition;
    m_pCondChecker[DIRECTIVE_COND_FIND_WEAK_ENEMY] = checkFindWeakEnemy;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_DEATH] = checkEnemyNotDeath;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_NOT_HALLUCINATION] = checkEnemyNotHallucination;
    m_pCondChecker[DIRECTIVE_COND_TIMING_MASTER_BLOOD_DRAIN] = checkTimingMasterBloodDrain;
    m_pCondChecker[DIRECTIVE_COND_TIMING_DUPLICATE_SELF] = checkTimingDuplicateSelf;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_RANGE_IN_MISSILE] = checkEnemyRangeInMissile;
    m_pCondChecker[DIRECTIVE_COND_POSSIBLE_SUMMON_MONSTERS] = checkPossibleSummonMonsters;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_TILE_NOT_ACID_SWAMP] = checkEnemyTileNotAcidSwamp;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_ON_AIR] = checkEnemyOnAir;
    m_pCondChecker[DIRECTIVE_COND_ENEMY_ON_SAFE_ZONE] = checkEnemyOnSafeZone;
    m_pCondChecker[DIRECTIVE_COND_CAN_ATTACK_THROWING_AXE] = checkCanAttackThrowingAxe;

    __END_CATCH
}

MonsterAI::~MonsterAI()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void MonsterAI::addDirective(Directive* pDirective)

{
    __BEGIN_TRY

    m_pDirectiveSet->addDirective(pDirective);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Condition functions
//////////////////////////////////////////////////////////////////////////////

bool MonsterAI::checkCondition(int condition, Creature* pEnemy)

{
    __BEGIN_TRY

    Assert(0 <= condition && condition < DIRECTIVE_COND_MAX);
    //	cout << m_pBody->toString() << " check condition " << condition << " to" << pEnemy->toString() << endl;
    return m_pCondChecker[condition](m_pBody, pEnemy);

    __END_CATCH
}

bool MonsterAI::checkDirective(Directive* pDirective, Creature* pEnemy)

{
    __BEGIN_TRY

    if (pDirective == NULL)
        return false;

    // Get the condition list.
    list<int>& conditionList = pDirective->getConditions();
    bool rValue = true;

    // If any one of the conditions is not satisfied,
    // return false.
    list<int>::iterator itr = conditionList.begin();
    for (; itr != conditionList.end(); itr++) {
        if (checkCondition(*itr, pEnemy) == false) {
            rValue = false;
            break;
        }
    }

    return rValue;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Normal movement
//////////////////////////////////////////////////////////////////////////////

bool MonsterAI::moveNormal(ZoneCoord_t ex, ZoneCoord_t ey, ZoneCoord_t& nx, ZoneCoord_t& ny, Dir_t& ndir)

{
    // Zone*       pZone   = m_pBody->getZone();
    Dir_t bestDir = DIR_NONE;

    Dir_t curDir = m_pBody->getDir();

    ////////////////////////////////////////////////////////////
    // Find the best direction toward the destination.
    ////////////////////////////////////////////////////////////
    if (m_pBody->getX() < ex) {
        if (m_pBody->getY() < ey)
            bestDir = RIGHTDOWN;
        else if (m_pBody->getY() > ey)
            bestDir = RIGHTUP;
        else
            bestDir = RIGHT; // m_pBody->getY() == ey
    } else if (m_pBody->getX() > ex) {
        if (m_pBody->getY() < ey)
            bestDir = LEFTDOWN;
        else if (m_pBody->getY() > ey)
            bestDir = LEFTUP;
        else
            bestDir = LEFT; // m_pBody->getY() == ey
    } else {
        if (m_pBody->getY() < ey)
            bestDir = DOWN; // m_pBody->getX() == ex
        else if (m_pBody->getY() > ey)
            bestDir = UP;
        else
            bestDir = DIR_NONE; // m_pBody->getY() == ey
    }

    ////////////////////////////////////////////////////////////
    // search surrounding tiles
    ////////////////////////////////////////////////////////////

    int diffLevel = 0;

    // Difference between two directions. It must be initialized to 0 so that diff == 0
    // when diffLevel == 1; otherwise a garbage value causes an error.
    int diff = 0;

    // Has the next direction been found?
    bool found = false;

    // Check bestDir first. This must not be an unsigned type.
    ndir = bestDir;

    // Next coordinates
    nx = m_pBody->getX() + dirMoveMask[ndir].x;
    ny = m_pBody->getY() + dirMoveMask[ndir].y;

    bool bBlocked[DIR_MAX] = {
        false,
    };

    const Dir_t DIR_MAX_1 = DIR_MAX - 1;

    // If bestDir is blocked, check by stepping +/- around the previous direction.
    // The zone boundary must be checked while doing so.
    bool bCanMove = m_pBody->canMove(nx, ny);

    // For a master, the ground must have nothing harmful on it.

    if (!bCanMove) {
        bBlocked[ndir] = true;

        // If the best direction is a wall, wall-following starts from here on.
        // Must not be a bat and must not be hiding.
        if (!m_pBody->isBlockedByCreature(nx, ny) && !m_pBody->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) &&
            !m_pBody->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            setMoveRule(MOVE_RULE_RIGHTWALL); // The value here does not matter.
            m_BlockedDir = bestDir;

            // For this turn it is enough to decide which direction to go.
        }

        while (true) {
            // On the first pass diffLevel == 1, which means check the m_pBody->getDir() direction.
            diffLevel++;

            // Stop when a suitable direction is found or there is nothing left to search.
            if (found || diffLevel > 8)
                break;

            ////////////////////////////////////////////////////////////
            // Two diffLevel values that are equal when divided by 2 denote directions
            // symmetric to each other. On an even diffLevel, diff is computed for the
            // first time and one side is checked; on an odd diffLevel the opposite side
            // was already checked in the previous loop, so the negated diff is used to
            // check the other side.
            ////////////////////////////////////////////////////////////
            if ((diffLevel & 0x00000001) == 0) // Equivalent to % 2.
            {
                // diff = (int)(diffLevel >> 1) * (bSelectLeft ? 1 : -1);
                diff = diffLevel >> 1;

                Dir_t dir1 = (m_pBody->getDir() + diff) & DIR_MAX_1;
                Dir_t dir2 = (m_pBody->getDir() + DIR_MAX - diff) & DIR_MAX_1;

                // Try the side closer to bestDir first.
                if ((abs(ndir + DIR_MAX - dir1) & DIR_MAX_1) < (abs(ndir + DIR_MAX - dir2) & DIR_MAX_1)) {
                    ndir = dir1;
                } else {
                    diff = -diff;
                    ndir = dir2;
                }
            } else
                ndir = m_pBody->getDir() + DIR_MAX - diff;

            ////////////////////////////////////////////////////////////
            // ndir must be corrected when it overflows or underflows.
            // overflow  : -1 -> 7 , -2 -> 6 , -3 -> 5 , ...
            // underflow :  8 -> 0 ,  9 -> 1 , 10 -> 2 , ...
            ////////////////////////////////////////////////////////////
            ndir &= DIR_MAX_1;

            // Compute the next coordinates.
            nx = m_pBody->getX() + dirMoveMask[ndir].x;
            ny = m_pBody->getY() + dirMoveMask[ndir].y;

            // If the next direction is empty, the loop can be exited.
            // The zone boundary must be checked while doing so.
            if (m_pBody->canMove(nx, ny)) {
                // For a master, the ground must have nothing harmful on it.
                { found = true; }
            }

            bBlocked[ndir] = true;
        } // while
    } else
        found = true;

    // Decide definitively between left-hand and right-hand wall following.
    if (found && m_MoveRule != MOVE_RULE_NORMAL) {
        bool leftWall = bBlocked[(ndir + 2) & DIR_MAX_1];
        bool rightWall = bBlocked[(ndir + DIR_MAX - 2) & DIR_MAX_1];

        // If both are blocked, choose according to the direction.
        if (leftWall && rightWall) {
            if ((ndir > curDir && ndir < curDir + 4) || (curDir > 4 && (ndir > curDir || ndir < curDir - 4))) {
                setMoveRule(MOVE_RULE_RIGHTWALL);
                // cout << "set RightWall : " << (int)curDir << " to " << (int)ndir << " - " << (int)m_BlockedDir << " -
                // " << m_pBody->getName() << endl;
            } else {
                setMoveRule(MOVE_RULE_LEFTWALL);
                // cout << "set LeftWall : " << (int)curDir << " to " << (int)ndir << " - " << (int)m_BlockedDir << " -
                // " << m_pBody->getName() << endl;
            }
        }
        // Apply the left-hand rule.
        else if (leftWall) {
            setMoveRule(MOVE_RULE_LEFTWALL);
            // cout << "set LeftWall : " << (int)curDir << " to " << (int)ndir << " - " << (int)m_BlockedDir << " - " <<
            // m_pBody->getName() << endl;
        }
        // Apply the right-hand rule.
        else if (rightWall) {
            setMoveRule(MOVE_RULE_RIGHTWALL);
            // cout << "set RightWall : " << (int)curDir << " to " << (int)ndir << " - " << (int)m_BlockedDir << " - "
            // << m_pBody->getName() << endl;
        }
        // Do not wall-follow when there is no wall.
        else {
            setMoveRule(MOVE_RULE_NORMAL);
        }
    } else {
        setMoveRule(MOVE_RULE_NORMAL);
    }

    return found;
}

//////////////////////////////////////////////////////////////////////////////
// Moving along a wall
//////////////////////////////////////////////////////////////////////////////

bool MonsterAI::moveWall(ZoneCoord_t ex, ZoneCoord_t ey, ZoneCoord_t& nx, ZoneCoord_t& ny, Dir_t& ndir, bool bLeft)

{
    // Zone*       pZone   = m_pBody->getZone();
    // cout << "MoveWall[" << (bLeft? "Left" : "Right") << "] : " << m_pBody->getName() << endl;

    ////////////////////////////////////////////////////////////
    // search surrounding tiles
    ////////////////////////////////////////////////////////////
    // First try to go in the target direction.
    ndir = m_BlockedDir;

    // Next coordinates
    nx = m_pBody->getX() + dirMoveMask[ndir].x;
    ny = m_pBody->getY() + dirMoveMask[ndir].y;

    m_WallCount++;

    // Whether the target direction can be entered
    bool bCanMove = m_pBody->canMove(nx, ny);

    // For a master, the ground must have nothing harmful on it.

    if (bCanMove) {
        // Stop wall-following only when the best direction after moving
        // one tile is not blocked.
        // if (m_pBody->canMove(bx,by))
        {
            // Stop wall-following.
            // cout << "FindBest : " << (int)m_BlockedDir << " - " << m_pBody->getName() << endl;
            setMoveRule(MOVE_RULE_NORMAL);
        }

        return true;
    }


    const Dir_t DIR_MAX_1 = DIR_MAX - 1;

    Dir_t dirInc, dirWallInc;

    if (bLeft) {
        // Left-hand rule
        dirInc = 7;     //	+DIR_MAX-1;
        dirWallInc = 2; // Direction the wall must be in
    } else {
        // Right-hand rule
        dirInc = 1;
        dirWallInc = 6; // +DIR_MAX-3 Direction the wall must be in
    }

    // Check starting from the direction the wall must be in.
    ndir = (m_pBody->getDir() + dirWallInc) & DIR_MAX_1;

    //----------------------------------------------------------
    // If the intended direction is passable, just go there.
    //
    // If it is impassable for a reason other than a map block,
    //        stop wall-following.
    //----------------------------------------------------------
    for (int i = 0; i < DIR_MAX; i++) {
        // Next coordinates
        nx = m_pBody->getX() + dirMoveMask[ndir].x;
        ny = m_pBody->getY() + dirMoveMask[ndir].y;

        if (m_pBody->canMove(nx, ny)) {
            // Just go there unconditionally.
            { return true; }
        }
        // If another creature blocks the way, stop wall-following.
        else if (m_pBody->isBlockedByCreature(nx, ny)) {
            break;
        }

        // Next direction
        ndir = (ndir + dirInc) & DIR_MAX_1;
    }

    // If wall-following cannot reach anywhere, stop wall-following.
    setMoveRule(MOVE_RULE_NORMAL);

    // cout << "Can't go Wall : " << m_pBody->getName() << endl;

    return false;
}


//////////////////////////////////////////////////////////////////////////////
// Action functions (attack related)
//////////////////////////////////////////////////////////////////////////////

bool MonsterAI::move(ZoneCoord_t ex, ZoneCoord_t ey)

{
    Zone* pZone = m_pBody->getZone();

    // Position and direction to move to
    ZoneCoord_t nx, ny;
    Dir_t ndir;

    // Decide where to move according to the move rule.
    bool found = false;
    switch (m_MoveRule) {
    // Normal movement
    case MOVE_RULE_NORMAL:
        found = moveNormal(ex, ey, nx, ny, ndir);
        break;

    // Left-hand rule
    case MOVE_RULE_LEFTWALL:
        found = moveWall(ex, ey, nx, ny, ndir, true); // true means the left direction.
        break;

    // Right-hand rule
    case MOVE_RULE_RIGHTWALL:
        found = moveWall(ex, ey, nx, ny, ndir, false); // false means the right direction.
        break;

    default:
        break;
    }

    // 1. The next direction was found, and
    // 2. it is not a safe zone.
    if ((found) && !(pZone->getZoneLevel(nx, ny) & SAFE_ZONE)) {
        // This changes the creature's coordinates and direction internally and broadcasts it.
        // (nx, ny) is already computed, so there is no need to recompute it.
        pZone->moveCreature(m_pBody, nx, ny, ndir);
    }

    m_LastAction = LAST_ACTION_MOVE;

    return true;
}

bool MonsterAI::move(Creature* pEnemy, bool bRetreat)

{
    __BEGIN_TRY

    Zone* pZone = m_pBody->getZone();
    ZoneCoord_t enemyX = pEnemy->getX();
    ZoneCoord_t enemyY = pEnemy->getY();
    ZoneCoord_t myX = m_pBody->getX();
    ZoneCoord_t myY = m_pBody->getY();
    int xOffset = enemyX - myX;
    int yOffset = enemyY - myY;
    ZoneCoord_t ex = pEnemy->getX();
    ZoneCoord_t ey = pEnemy->getY();

    // When fleeing, act as though the enemy stood at the coordinates opposite
    // the enemy, and move.
    if (bRetreat) {
        ////////////////////////////////////////////////////////////
        // (enemyX, enemyY)
        //
        //                  myX, myY
        //
        //                           (ex, ey)
        //
        // Flee in the diagonally opposite direction from the position the enemy
        // is at (enemyX, enemyY).
        ////////////////////////////////////////////////////////////
        int xOffset2 = xOffset << 1; // by sigi
        int yOffset2 = yOffset << 1; // by sigi

        if (enemyX - xOffset2 < 0)
            ex = 0;
        else if (enemyX - xOffset2 > (int)pZone->getWidth())
            ex = pZone->getWidth();
        else
            ex = enemyX - xOffset2;

        if (enemyY - yOffset2 < 0)
            ey = 0;
        else if (enemyY - yOffset2 > (int)pZone->getHeight())
            ey = pZone->getHeight();
        else
            ey = enemyY - yOffset2;

        setMoveRule(MOVE_RULE_NORMAL); // Normal movement
    }

    return move(ex, ey);

    __END_CATCH
}

bool MonsterAI::flee(Creature* pEnemy)

{
    __BEGIN_TRY

    if (m_Panic == 0) {
        if (m_Courage == 0) {
            m_Panic = m_PanicMax;
            m_Courage = m_CourageMax;
        } else {
            m_Courage--;
        }
    } else
        m_Panic--;

    if (m_Panic > 0) {
        // Flee.
        move(pEnemy, true);
        return true;
    }

    return false;

    __END_CATCH
}

int MonsterAI::useSkill(Creature* pEnemy, SkillType_t SkillType, int ratio)

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    Assert(pEnemy != NULL);

    ZoneCoord_t ex = pEnemy->getX();
    ZoneCoord_t ey = pEnemy->getY();
    Distance_t dist = m_pBody->getDistance(ex, ey);

    // Each action is only taken with its own probability.
    if (rand() % 100 >= ratio)
        return SKILL_FAILED_RATIO;

    // [ test code ]
    //	Zone* pZone = m_pBody->getZone();
    // Assert( pZone != NULL );

    // cout << "ZoneID=" << (int)pZone->getZoneID()
    //     << ", MonsterCount=" << (int)pZone->getMonsterCount() << endl;


    //----------------------------------------------------------
    // If the state makes using a skill impossible,
    // it must first be made possible.
    // This could be put into Directive, but it is the same for every monster.
    //
    // It may need to be split into Slayer monsters and Vampire monsters.
    //----------------------------------------------------------
    // If hidden, come out.
    if (m_pBody->isFlag(Effect::EFFECT_CLASS_HIDE)) {
        SkillType = SKILL_UN_BURROW;
    }
    // If in bat form, transform back.
    else if (m_pBody->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        SkillType = SKILL_UN_TRANSFORM;
    }
    // If invisible, the invisibility must be removed.
    else if (m_pBody->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
        SkillType = SKILL_UN_INVISIBILITY;
    }
    // A wolf should not be able to cast magic.

    SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SkillType);
    Assert(pSkillHandler != NULL);

    SkillInfo* pSkillInfo = NULL;
    SkillInfoManager& skillInfos = de::gameContext().skillInfos();

    Timeval delay;

    switch (SkillType) {
    ////////////////////////////////////////////////////////////
    // Melee skills
    ////////////////////////////////////////////////////////////
    case SKILL_ATTACK_MELEE:
    case SKILL_ACID_TOUCH:
    case SKILL_SINGLE_BLOW:
    case SKILL_BLOODY_NAIL:
    case SKILL_TALON_OF_CROW:
        if (dist > m_pBody->getMeleeRange())
            return SKILL_FAILED_RANGE;
        pSkillHandler->execute(m_pBody, pEnemy);
        break;
    case SKILL_BLOOD_DRAIN:
        if (!m_pBody->isMaster() && dist > m_pBody->getMeleeRange())
            return SKILL_FAILED_RANGE;
        pSkillHandler->execute(m_pBody, pEnemy);
        break;
    ////////////////////////////////////////////////////////////
    // Ranged skills
    ////////////////////////////////////////////////////////////
    case SKILL_ATTACK_ARMS:
        if (dist > m_pBody->getMissileRange())
            return SKILL_FAILED_RANGE;
        pSkillHandler->execute(m_pBody, pEnemy);
        break;
    ////////////////////////////////////////////////////////////
    // Ranged skills (with delay)
    ////////////////////////////////////////////////////////////
    case SKILL_GREEN_POISON:
    case SKILL_YELLOW_POISON:
    case SKILL_DARKBLUE_POISON:
    case SKILL_GREEN_STALKER:
    case SKILL_ACID_BOLT:
    case SKILL_ACID_BALL:
    case SKILL_BLOODY_BALL:
    case SKILL_BLOODY_WAVE:
    case SKILL_BLOODY_MASTER_WAVE:
    case SKILL_BLOODY_WARP:
    case SKILL_PARALYZE:
    case SKILL_DOOM:
    case SKILL_SEDUCTION:
    case SKILL_DEATH:
    case SKILL_DARKNESS:
    case SKILL_BLOODY_KNIFE:
    case SKILL_BLOODY_SPEAR:
    case SKILL_BLOODY_STORM:
    case SKILL_ACID_STORM:
    case SKILL_POISON_STORM:
    case SKILL_HALLUCINATION:
    // Added so that monsters can use these.
    case SKILL_METEOR_STRIKE:
    case SKILL_BLOODY_WALL:
    case SKILL_BLOODY_SNAKE:
    case SKILL_BLOODY_BREAKER:
    case SKILL_ACID_SWAMP:
    case SKILL_POISON_STRIKE:
    case SKILL_ACID_STRIKE:
    case SKILL_HANDS_OF_WISDOM:
    case SKILL_POISONOUS_HANDS:
    case SKILL_BLOODY_ZENITH:
    case SKILL_ACID_ERUPTION:
    case SKILL_ICE_FIELD:

    // Gilles de Rais skills
    case SKILL_WIDE_ICE_FIELD:
    case SKILL_GLACIER_1:
    case SKILL_GLACIER_2:
    case SKILL_ICE_AUGER:
    case SKILL_ICE_HAIL:
    case SKILL_WIDE_ICE_HAIL:
    case SKILL_ICE_WAVE:
    case SKILL_ICE_LANCE:
    case SKILL_GUN_SHOT_GUIDANCE:
    case SKILL_THROWING_AXE:
    case SKILL_CHOPPING_FIREWOOD:
    case SKILL_CHAIN_THROWING_AXE:
    case SKILL_MULTI_THROWING_AXE:
    case SKILL_CANNONADE:
    case SKILL_AR_ATTACK:
    case SKILL_SMG_ATTACK:
    case SKILL_GRENADE_ATTACK:
    case SKILL_TRASLA_ATTACK:
    case SKILL_PUSCA_ATTACK:
    case SKILL_NOD_COPILA_ATTACK:
    case SKILL_NOD_COPILA_ATTACK_2:

        pSkillInfo = skillInfos.getSkillInfo(SkillType);
        Assert(pSkillInfo != NULL);

        if (SkillType == SKILL_GUN_SHOT_GUIDANCE || SkillType == SKILL_ICE_LANCE || SkillType == SKILL_ICE_HAIL) {
            // No range check for these skills.
        } else if (dist > pSkillInfo->getRange())
            return SKILL_FAILED_RANGE;
        // cout << "useSkill: " << pSkillInfo->getName().c_str() << endl;
        pSkillHandler->execute(m_pBody, pEnemy);
        // cout << "useSkillOK: " << pSkillInfo->getName().c_str() << endl;

        // Missiles fire too fast, so a delay is set.
        if (m_pBody->isMaster() || m_pBody->getMonsterType() == 599 || m_pBody->getMonsterType() == 764 ||
            m_pBody->getMonsterType() == 765) // Masters get a shorter delay.
        {
            delay.tv_sec = 0;
            delay.tv_usec = 700000;
        } else {
            delay.tv_sec = 1;
            delay.tv_usec = 500000;
        }

        if (SkillType == SKILL_ICE_WAVE || SkillType == SKILL_CHOPPING_FIREWOOD) {
            delay.tv_sec = 2;
            delay.tv_usec = 500000;
        }
        m_pBody->addAccuDelay(delay);
        break;
    ////////////////////////////////////////////////////////////
    // Self skills
    ////////////////////////////////////////////////////////////
    case SKILL_HIDE:
    case SKILL_UN_BURROW:
    case SKILL_UN_TRANSFORM:
    case SKILL_TRANSFORM_TO_WOLF:
    case SKILL_TRANSFORM_TO_BAT:
    case SKILL_INVISIBILITY:
    case SKILL_UN_INVISIBILITY:
    case SKILL_DUPLICATE_SELF:
        pSkillHandler->execute(m_pBody);

        // Set the delay.
        delay.tv_sec = 1;
        delay.tv_usec = 500000;
        m_pBody->addAccuDelay(delay);
        break;

    case SKILL_SUMMON_MONSTERS:
        if (!checkPossibleSummonMonsters(m_pBody, NULL) && !m_pBody->isMaster())
            return SKILL_FAILED_RANGE;

        pSkillHandler->execute(m_pBody);

        // Set the delay.
        delay.tv_sec = 1;
        delay.tv_usec = 500000;
        m_pBody->addAccuDelay(delay);
        break;

    case SKILL_CRITICAL_GROUND:
        pSkillInfo = skillInfos.getSkillInfo(SkillType);
        Assert(pSkillInfo != NULL);

        // cout << "useSkill: " << pSkillInfo->getName().c_str() << endl;
        pSkillHandler->execute(m_pBody);
        // cout << "useSkillOK: " << pSkillInfo->getName().c_str() << endl;

        // Set the delay.
        delay.tv_sec = 1;
        delay.tv_usec = 0;
        m_pBody->addAccuDelay(delay);
        break;

    case SKILL_PLAYING_WITH_FIRE:
    case SKILL_SUMMON_MIGA_ATTACK:
    case SKILL_SELF_DESTRUCTION:
        pSkillInfo = skillInfos.getSkillInfo(SkillType);
        Assert(pSkillInfo != NULL);

        // cout << "useSkill: " << pSkillInfo->getName().c_str() << endl;
        pSkillHandler->execute(m_pBody);
        // cout << "useSkillOK: " << pSkillInfo->getName().c_str() << endl;

        // Set the delay.
        delay.tv_sec = 3;
        delay.tv_usec = 0;
        m_pBody->addAccuDelay(delay);
        break;

    case SKILL_GORE_GLAND_FIRE:
        pSkillInfo = skillInfos.getSkillInfo(SkillType);
        Assert(pSkillInfo != NULL);

        if (dist > pSkillInfo->getRange())
            return SKILL_FAILED_RANGE;
        pSkillHandler->execute(m_pBody, pEnemy);

        break;

    ////////////////////////////////////////////////////////////
    // QuickFire - -;
    ////////////////////////////////////////////////////////////
    case SKILL_QUICK_FIRE:
        pSkillInfo = skillInfos.getSkillInfo(SkillType);
        Assert(pSkillInfo != NULL);

        if (dist > pSkillInfo->getRange())
            return SKILL_FAILED_RANGE;
        pSkillHandler->execute(m_pBody, pEnemy);

        break;


    ////////////////////////////////////////////////////////////
    // Unsupported skill
    ////////////////////////////////////////////////////////////
    default:
        cerr << "MonsterAI::useSkill() : Try to use unavailable skill!" << endl;
        Assert(false);
        break;
    }

    m_LastAction = LAST_ACTION_SKILL;

    return 0;

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Action pattern selection function
//////////////////////////////////////////////////////////////////////////////

void MonsterAI::deal(Creature* pEnemy, const Timeval& currentTime)

{
    __BEGIN_TRY

    Assert(pEnemy != NULL); // by sigi

    // Get the directive list.
    const list<Directive*>& directiveList = m_pDirectiveSet->getDirectives();
    int rValue = 0;
    int parameter = 0;
    int ratio = 0;

    // Walk the directive list one entry at a time and check
    // whether any of them has its conditions satisfied.
    list<Directive*>::const_iterator itr = directiveList.begin();
    for (; itr != directiveList.end(); itr++) {
        Directive* pDirective = *itr;

        // If a directive's conditions are satisfied,
        // perform the action it names.
        if (checkDirective(pDirective, pEnemy)) {
            switch (pDirective->getAction()) {
            // Approach the enemy.
            case DIRECTIVE_ACTION_APPROACH:
                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_APPROACH");
                approach(pEnemy);
                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_APPROACH");
                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] approach" << endl;
                break;

            // Flee from the enemy.
            case DIRECTIVE_ACTION_FLEE:
                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_FLEE");
                if (!flee(pEnemy)) {
                    setMoveRule(MOVE_RULE_NORMAL); // Normal movement
                    rValue = useSkill(pEnemy, SKILL_ATTACK_MELEE, 100);
                    if (rValue != 0)
                        approach(pEnemy);
                    // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] !free" << endl;
                } else {
                    // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] free" << endl;
                }
                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_FLEE");
                break;

            // Use a skill: attack the enemy, or a self skill.
            case DIRECTIVE_ACTION_USE_SKILL:
                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_USE_SKILL");
                // Skills cannot be used while Block Head is applied.
                if (m_pBody->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) || m_pBody->isFlag(Effect::EFFECT_CLASS_TENDRIL))
                    continue;
                parameter = pDirective->getParameter();
                ratio = pDirective->getRatio();
                rValue = useSkill(pEnemy, (SkillType_t)parameter, ratio);
                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_USE_SKILL");

                if (rValue != 0) {
                    // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] !" <<
                    // SkillTypes2String[parameter] << endl;
                    continue;
                }

                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] " <<
                // SkillTypes2String[parameter] << endl;


                setMoveRule(MOVE_RULE_NORMAL); // Normal movement

                break;

            // Give up on the enemy.
            case DIRECTIVE_ACTION_FORGET:
                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_FORGET");
                // by sigi. 2002.10.7
                if (!m_pBody->getEnemies().empty())
                    m_pBody->getEnemies().pop_front();

                setMoveRule(MOVE_RULE_NORMAL); // Normal movement
                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] forget" << endl;
                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_FORGET");

                break;

            // Find a new PrimaryEnemy and check it.
            case DIRECTIVE_ACTION_CHANGE_ENEMY: {
                ratio = pDirective->getParameter();

                // Do not change the enemy.
                if (rand() % 100 >= ratio) {
                    // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] !changeEnemy" <<
                    // endl;
                    continue;
                }

                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_CHANGE_ENEMY");

                Creature* pNewEnemy = m_pBody->getPrimaryEnemy();
                if (pNewEnemy != NULL) {
                    pEnemy = pNewEnemy;
                } else {
                    // If empty, checkFindWeakEnemy deleted the enemies and no new
                    // enemy was added.
                    // Re-add the original enemy.
                    // by sigi. 2002.10.7
                    if (m_pBody->getEnemies().empty()) {
                        m_pBody->addEnemy(pEnemy);
                    }
                }

                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_CHANGE_ENEMY");

                // Continue on to the next AI step.
                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] changeEnemy" << endl;
                continue;
            } break;

            // Move somewhere nearby.
            case DIRECTIVE_ACTION_MOVE_RANDOM: {
                ratio = pDirective->getParameter();

                // Do not move anywhere.
                if (rand() % 100 >= ratio) {
                    // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] !moveRandom" <<
                    // endl;
                    continue;
                }

                ZoneCoord_t x;
                ZoneCoord_t y;
                TPOINT p;

                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_MOVE_RANDOM_GET_SAFE_TILE");

                x = m_pBody->getX();
                y = m_pBody->getY();

                p = getSafeTile(m_pBody->getZone(), x, y);

                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_MOVE_RANDOM_GET_SAFE_TILE");

                // If there is no free spot, give up.
                if (p.x == -1) {
                    /// cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] moveRandom : no
                    /// position" << endl;
                    continue;
                }

                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_MOVE_RANDOM_MOVE");

                ZoneCoord_t x1 = p.x; // x + rand()%7 - 3;
                ZoneCoord_t y1 = p.y; // y + rand()%7 - 3;

                if (x != x1 || y != y1)
                    move(x1, y1);

                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_MOVE_RANDOM_MOVE");
                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] moveRandom" << endl;
            } break;

            // Wait for a while.
            case DIRECTIVE_ACTION_WAIT: {
                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_WAIT");
                Timeval delay;
                delay.tv_sec = 2;
                delay.tv_usec = 0;
                m_pBody->addAccuDelay(delay);
                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_WAIT");
                // cout << "[" << (int)currentTime.tv_sec << "." << (int)currentTime.tv_usec << "] wait" << endl;
            } break;

            case DIRECTIVE_ACTION_FAST_FLEE: {
                bool result = false;

                __BEGIN_PROFILE_MONSTER("DIRECTIVE_ACTION_FAST_FLEE");
                int myX = m_pBody->getX();
                int myY = m_pBody->getY();

                int nmX = pEnemy->getX();
                int nmY = pEnemy->getY();

                int diffX = myX - nmX;
                int diffY = myY - nmY;

                float ratio = 5.0 / (ABS(diffX) + ABS(diffY));

                int newX = myX + diffX * ratio;
                int newY = myY + diffY * ratio;

                if (isValidZoneCoord(m_pBody->getZone(), newX, newY))
                    result = m_pBody->getZone()->moveFastMonster(m_pBody, myX, myY, newX, newY, SKILL_RAPID_GLIDING);

                __END_PROFILE_MONSTER("DIRECTIVE_ACTION_FAST_FLEE");

                if (!result)
                    continue;
            } break;
            case DIRECTIVE_ACTION_SAY: {
                parameter = pDirective->getParameter();
                GCSay gcSay;
                gcSay.setObjectID(m_pBody->getObjectID());
                gcSay.setMessage(de::gameContext().strings().getString(parameter));
                gcSay.setColor(0x00ffffff);
                m_pBody->getZone()->broadcastPacket(m_pBody->getX(), m_pBody->getY(), &gcSay);
            }
            }
            break;
        }
    }

    __BEGIN_PROFILE_MONSTER("MAI_POST_DEAL");

    // Set the action delay according to which action was taken this turn.
    switch (m_LastAction) {
    case LAST_ACTION_NONE:
    case LAST_ACTION_MOVE:
        setDelay(currentTime);
        break;
    case LAST_ACTION_SKILL:
        setAttackDelay(currentTime);
        break;
    }

    // Reset the action variable.
    m_LastAction = LAST_ACTION_NONE;

    // Reset the flee-related variable at random.
    if ((rand() & 0x0000007F) > 64) //%100 > 50)
    {
        if (m_bDamaged) {
            m_bDamaged = false;
            m_Panic = m_PanicMax;
            m_Courage = m_CourageMax;
        }
    }

    __END_PROFILE_MONSTER("MAI_POST_DEAL");

    __END_CATCH
}

// Take the action performed on death.
void MonsterAI::actDeadAction(Creature* pEnemy)

{
    __BEGIN_TRY

    // Because of MonsterAI's limits, the actions that can be performed
    // when there is no enemy are limited.
    if (pEnemy != NULL) {
        // Get the directive list.
        const list<Directive*>& directiveList = m_pDirectiveSet->getDeadDirectives();
        bool bContinue = true;

        // Walk the directive list one entry at a time and check
        // whether any of them has its conditions satisfied.
        list<Directive*>::const_iterator itr = directiveList.begin();
        for (; itr != directiveList.end() && bContinue; itr++) {
            Directive* pDirective = *itr;

            if (pDirective->getAction() == DIRECTIVE_ACTION_USE_SKILL) {
                SkillType_t SkillType = pDirective->getParameter();
                int ratio = pDirective->getRatio();

                SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

                if ((rand() % ratio) < 100) {
                    switch (SkillType) {
                    case SKILL_GREEN_POISON:
                    case SKILL_LAND_MINE_EXPLOSION:
                    case SKILL_PLEASURE_EXPLOSION:
                    case SKILL_CLAYMORE_EXPLOSION:
                    case SKILL_SUMMON_MONSTERS:
                        pSkillHandler->execute(m_pBody);
                        bContinue = false;
                        break;
                    default:
                        Assert(false);
                        break;
                    }
                } else {
                    // If the skill failed its probability check, move on to the next action.
                    continue;
                }
            } else if (pDirective->getAction() == DIRECTIVE_ACTION_SAY) {
                int parameter = pDirective->getParameter();
                GCSay gcSay;
                gcSay.setObjectID(m_pBody->getObjectID());
                gcSay.setMessage(de::gameContext().strings().getString(parameter));
                gcSay.setColor(0x00ffffff);
                m_pBody->getZone()->broadcastPacket(m_pBody->getX(), m_pBody->getY(), &gcSay);
            }
        }
    }

    __END_CATCH
}

void MonsterAI::setDelay(const Timeval& currentTime)

{
    __BEGIN_TRY

    // Reuse the time obtained earlier, to save computation.
    // Timeval currentTime;
    // getCurrentTime(currentTime);

    int delay = m_pBody->getDelay();

    if (m_pBody->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        delay = 200; // 250
    } else if (m_pBody->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        delay = 300; // 375
    }

    int modifier = (rand() % 41) - 20;

    delay = delay * 1000;
    delay = delay + delay * modifier / 100;

    Timeval nexttime;
    nexttime.tv_sec = delay / 1000000;
    nexttime.tv_usec = delay % 1000000;

    nexttime = nexttime + m_pBody->getAccuDelay();

    m_pBody->clearAccuDelay();

    if (m_pBody->isFlag(Effect::EFFECT_CLASS_ICE_FIELD_TO_CREATURE) ||
        m_pBody->isFlag(Effect::EFFECT_CLASS_JABBING_VEIN)) {
        m_pBody->setNextTurn(currentTime + nexttime + nexttime);
    } else {
        m_pBody->setNextTurn(currentTime + nexttime);
    }

    __END_CATCH
}

void MonsterAI::setAttackDelay(const Timeval& currentTime)

{
    __BEGIN_TRY

    // Reuse the time obtained earlier, to save computation.
    // Timeval currentTime;
    // getCurrentTime(currentTime);

    int delay = m_pBody->getAttackDelay();
    int modifier = (rand() % 21);

    delay = delay * 1000;
    delay = delay + delay * modifier / 100;

    Timeval nexttime;
    nexttime.tv_sec = delay / 1000000;
    nexttime.tv_usec = delay % 1000000;

    nexttime = nexttime + m_pBody->getAccuDelay();

    m_pBody->clearAccuDelay();

    if (m_pBody->isFlag(Effect::EFFECT_CLASS_ICE_OF_SOUL_STONE) ||
        m_pBody->getZone()->getTile(m_pBody->getX(), m_pBody->getY()).getEffect(Effect::EFFECT_CLASS_FURY_OF_GNOME) !=
            NULL) {
        m_pBody->setNextTurn(currentTime + nexttime + nexttime);
    } else {
        m_pBody->setNextTurn(currentTime + nexttime);
    }


    __END_CATCH
}


bool checkEnemyRangeMelee(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist <= pMonster->getMeleeRange())
        return true;
    return false;
}

bool checkEnemyRangeMissile(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist > pMonster->getMeleeRange() && dist <= pMonster->getSight())
        return true;
    return false;
}

bool checkEnemyRangeInMissile(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist <= pMonster->getSight())
        return true;
    return false;
}

bool checkEnemyRangeClose(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist <= 1)
        return true;
    return false;
}


bool checkEnemyRangeOutOfSight(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist > pMonster->getSight())
        return true;
    return false;
}

bool checkEnemyDying(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;

    HP_t EnemyCurHP = 0;
    HP_t EnemyMaxHP = 0;

    if (pEnemy->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pEnemy);
        EnemyCurHP = pSlayer->getHP(ATTR_CURRENT);
        EnemyMaxHP = pSlayer->getHP(ATTR_MAX);
    } else if (pEnemy->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pEnemy);
        EnemyCurHP = pVampire->getHP(ATTR_CURRENT);
        EnemyMaxHP = pVampire->getHP(ATTR_MAX);
    } else if (pEnemy->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pEnemy);
        EnemyCurHP = pOusters->getHP(ATTR_CURRENT);
        EnemyMaxHP = pOusters->getHP(ATTR_MAX);
    } else if (pEnemy->isMonster()) {
        Monster* pEnemyMonster = dynamic_cast<Monster*>(pEnemy);
        EnemyCurHP = pEnemyMonster->getHP(ATTR_CURRENT);
        EnemyMaxHP = pEnemyMonster->getHP(ATTR_MAX);
    }

    if (EnemyCurHP * 5 < EnemyMaxHP)
        return true;

    return false;
}

bool checkEnemyNotBloodDrained(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN))
        return true;
    return false;
}

bool checkEnemyNotGreenPoisoned(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_POISON))
        return true;
    return false;
}

bool checkEnemyNotYellowPoisoned(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE))
        return true;
    return false;
}

bool checkEnemyNotDarkbluePoisoned(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_DARKBLUE_POISON))
        return true;
    return false;
}

bool checkEnemyNotGreenStalkered(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_GREEN_STALKER))
        return true;
    return false;
}

bool checkEnemyNotParalyzed(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_PARALYZE))
        return true;
    return false;
}

bool checkEnemyNotDoomed(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_DOOM))
        return true;
    return false;
}

bool checkEnemyNotBlinded(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return false;
}

bool checkEnemyNotInDarkness(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (pEnemy->isVampire())
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_DARKNESS))
        return true;
    return false;
}

bool checkEnemyNotSeduction(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_SEDUCTION))
        return true;
    return false;
}

bool checkEnemyNotDeath(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_DEATH))
        return true;
    return false;
}

bool checkImOK(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    HP_t CurHP = pMonster->getHP(ATTR_CURRENT);
    HP_t MaxHP = pMonster->getHP(ATTR_MAX);
    if (CurHP > MaxHP / 3)
        return true;
    return false;
}

bool checkImDying(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    HP_t CurHP = pMonster->getHP(ATTR_CURRENT);
    HP_t MaxHP = pMonster->getHP(ATTR_MAX);
    if (CurHP * 4 < MaxHP)
        return true;
    return false;
}

bool checkImDamaged(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return pMonster->getBrain()->getDamaged();
}

bool checkImHiding(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return pMonster->isFlag(Effect::EFFECT_CLASS_HIDE);
}

bool checkImWolf(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF);
}

bool checkImBat(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT);
}

bool checkImInvisible(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    return pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY);
}


bool checkImWalkingWall(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    MonsterAI* pAI = pMonster->getBrain();

    Assert(pAI != NULL);

    if (pAI->getWallCount() > 3 &&
        (pAI->getMoveRule() == MOVE_RULE_LEFTWALL || pAI->getMoveRule() == MOVE_RULE_RIGHTWALL)) {
        return true;
    }

    return false;
}

bool checkTimingBloodDrain(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL
        // Do not drain blood from an NPC.
        || pEnemy->isNPC()
        // Do not drain blood from a target that has already been drained.
        || pEnemy->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)
        // Invulnerable, or immune to blood drain.
        || pEnemy->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE) ||
        pEnemy->isFlag(Effect::EFFECT_CLASS_IMMUNE_TO_BLOOD_DRAIN)) {
        return false;
    }

    // Check whether the target is within blood drain range.
    Distance_t dist = pMonster->getDistance(pEnemy->getX(), pEnemy->getY());
    if (dist > pMonster->getMeleeRange())
        return false;

    HP_t EnemyCurHP = 0;
    HP_t EnemyMaxHP = 0;

    if (pEnemy->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pEnemy);
        EnemyCurHP = pSlayer->getHP(ATTR_CURRENT);
        EnemyMaxHP = pSlayer->getHP(ATTR_MAX);
    } else if (pEnemy->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pEnemy);
        EnemyCurHP = pVampire->getHP(ATTR_CURRENT);
        EnemyMaxHP = pVampire->getHP(ATTR_MAX);
    } else if (pEnemy->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pEnemy);
        EnemyCurHP = pOusters->getHP(ATTR_CURRENT);
        EnemyMaxHP = pOusters->getHP(ATTR_MAX);
    } else if (pEnemy->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pEnemy);
        EnemyCurHP = pMonster->getHP(ATTR_CURRENT);
        EnemyMaxHP = pMonster->getHP(ATTR_MAX);
    }

    // Do not drain blood while 1/5 or more of HP remains.
    if (EnemyCurHP * 5 >= EnemyMaxHP)
        return false;

    // Finally, drain blood.
    return true;
}

bool checkTimingMasterBloodDrain(Monster* pMonster, Creature* pEnemy) {
    // 20% check
    // The computation itself is expensive.
    // if (rand()%100 > 10)
    //	return false;

    Assert(pMonster != NULL);
    VariableManager& variables = de::gameContext().variables();

    if (pEnemy == NULL) {
        return false;
    }

    int masterHPPercent = pMonster->getHP(ATTR_CURRENT) * 100 / pMonster->getHP(ATTR_MAX);

    // Blood drain probability
    // int startHPPercent 	= 70,	startBDPercent 	= 10;
    // int endHPPercent 	= 30, 	endBDPercent 	= 30;
    int startHPPercent = variables.getMasterBloodDrainStartHP();
    int startBDPercent = variables.getMasterBloodDrainStartBD();
    int endHPPercent = variables.getMasterBloodDrainEndHP();
    int endBDPercent = variables.getMasterBloodDrainEndBD();

    // Do not drain blood when HP is 70% or more.
    if (masterHPPercent >= startHPPercent)
        return false;

    int ratio = rand() % 100;

    // At 30% HP or less, drain blood with a 30% probability.
    if (masterHPPercent <= endHPPercent) {
        return ratio < endBDPercent;
    }

    // Between 30% and 70% HP, drain blood with a 10% to 30% probability.
    int maxBDPercent = max(startBDPercent, endBDPercent);
    int gapHPPercent = startHPPercent - endHPPercent;
    int gapBDPercent = abs(endBDPercent - startBDPercent);

    // int permitRatio = maxBDPercent - (masterHPPercent-endHPPercent)/2;
    int permitRatio = maxBDPercent - gapBDPercent * (masterHPPercent - endHPPercent) / gapHPPercent;

    return ratio < permitRatio;

    // Anyone at all can be drained.
    // return true;

    return true;
}

//----------------------------------------------------------------------
//
// bool checkMasterSummonTiming(Monster* pMonster, Creature* pEnemy)
//
//----------------------------------------------------------------------
// Conditions for using the summon skill:
// the current zone must be a master lair, and
// there must be no other monsters.
//----------------------------------------------------------------------
bool checkMasterSummonTiming(Monster* pMonster, Creature* pEnemy) {
    // cout << "checkMasterSummonTiming: ";

    Assert(pMonster != NULL);

    // Pointless if this is not a master.
    if (!pMonster->isMaster())
    //|| !pMonster->hasNextMonsterSummonInfo())
    {
        // cout << "not Master" << endl;
        return false;
    }

    // Check whether the zone the master is in is a master lair.
    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);

    // Meaningless if it is not a master lair.
    if (!pZone->isMasterLair()) {
        // cout << "not MasterLair" << endl;
        return false;
    }

    MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
    Assert(pMasterLairManager != NULL);

    // Summoning is possible when the master is not yet fighting and
    // the zone holds no monster other than the master.
    bool bSummonTiming = !pMasterLairManager->isMasterReady() && pZone->getMonsterManager()->getSize() <= 1;

    return bSummonTiming;
}

//----------------------------------------------------------------------
//
// bool checkMasterNotReady(Monster* pMonster, Creature* pEnemy)
//
//----------------------------------------------------------------------
bool checkMasterNotReady(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    // Pointless if this is not a master.
    if (!pMonster->isMaster()) {
        return false;
    }

    // Check whether the zone the master is in is a master lair.
    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);

    // Meaningless if it is not a master lair.
    if (!pZone->isMasterLair()) {
        return false;
    }

    MasterLairManager* pMasterLairManager = pZone->getMasterLairManager();
    Assert(pMasterLairManager != NULL);

    // Summoning is possible while the lair's master is not ready yet.
    return !pMasterLairManager->isMasterReady();
}

//----------------------------------------------------------------------
//
// bool checkImInBadPosition(Monster* pMonster, Creature* pEnemy)
//
//----------------------------------------------------------------------
// Is the current position a bad one?
//
// True when AcidSwamp is spread over the tile pMonster stands on.
//----------------------------------------------------------------------
bool checkImInBadPosition(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);

    Tile& rTile = pZone->getTile(pMonster->getX(), pMonster->getY());

    // Not invulnerable, and
    if (!pMonster->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE)

        // not immune to acid while AcidSwamp lies on the ground.
        && ((!pMonster->isFlag(Effect::EFFECT_CLASS_IMMUNE_TO_ACID) &&
             rTile.getEffect(Effect::EFFECT_CLASS_ACID_SWAMP) != NULL)

            || rTile.getEffect(Effect::EFFECT_CLASS_BLOODY_WALL) != NULL ||
            rTile.getEffect(Effect::EFFECT_CLASS_GROUND_ATTACK) != NULL)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------
//
// bool checkFindWeakEnemy(Monster* pMonster, Creature* pEnemy)
//
//----------------------------------------------------------------------
// Can a weaker enemy be found?
//
// If a nearby PC has a lower strength value than the current enemy,
// that PC becomes the monster's only enemy.
//----------------------------------------------------------------------
bool checkFindWeakEnemy(Monster* pMonster, Creature* pEnemy) {
    // For now
    // return false;

    try {
        if (pEnemy == NULL)
            return false;

        Assert(pMonster != NULL);

        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        int strongValue = getStrongValue(pEnemy);

        if (strongValue == 0)
            return false;

        // Judge the current enemy to be too strong and
        // look for a new, weaker enemy.
        Creature* pWeakestCreature = NULL;

        // Search the surroundings.
        ZoneCoord_t cx = pMonster->getX();
        ZoneCoord_t cy = pMonster->getY();
        ;
        ZoneCoord_t ix = 0;
        ZoneCoord_t iy = 0;
        ZoneCoord_t endx = 0;
        ZoneCoord_t endy = 0;

        // Search within the monster's sight range.
        Sight_t sight = pMonster->getSight();

        endx = min(pZone->getWidth() - 1, cx + sight + 1);
        endy = min(pZone->getHeight() - 1, cy + sight + 1);

        for (ix = max(0, cx - sight - 1); ix <= endx; ix++) {
            for (iy = max(0, cy - sight - 1); iy <= endy; iy++) {
                Tile& rTile = pZone->getTile(ix, iy);

                // Only when the tile holds a creature
                if (rTile.hasCreature()) {
                    const forward_list<Object*>& objectList = rTile.getObjectList();

                    for (forward_list<Object*>::const_iterator itr = objectList.begin();
                         itr != objectList.end() && (*itr)->getObjectPriority() <= OBJECT_PRIORITY_BURROWING_CREATURE;
                         itr++) {
                        Creature* pCreature = dynamic_cast<Creature*>(*itr);
                        Assert(pCreature != NULL);

                        // Neither pMonster nor pEnemy:
                        // a Player becomes a target.
                        if (pCreature != pMonster && pCreature != pEnemy && pCreature->isPC() &&
                            pMonster->isRealEnemy(pCreature)) {
                            int checkStrongValue = getStrongValue(pCreature);

                            // Appoint the weaker one as pWeakestCreature.
                            if (checkStrongValue < strongValue) {
                                pWeakestCreature = pCreature;
                                strongValue = checkStrongValue;
                            }
                        }
                    }
                }
            }
        }

        // If the weakest one was found
        if (pWeakestCreature != NULL) {
            pMonster->deleteAllEnemy();
            pMonster->addEnemy(pWeakestCreature);

            return true;
        } else {
        }

    } catch (Throwable& t) {
        // Ignore every exception.
        filelog("monsterAIBug.txt", "%s", t.toString().c_str());
    }

    return false;
}

bool checkEnemyNotHallucination(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    if (pEnemy == NULL)
        return false;
    if (!pEnemy->isFlag(Effect::EFFECT_CLASS_HALLUCINATION))
        return true;
    return false;
}

bool checkTimingDuplicateSelf(Monster* pMonster, Creature* pEnemy) {
    Assert(pMonster != NULL);

    // Pointless if this is not a master.
    if (!pMonster->isMaster()) {
        return false;
    }

    // Check whether the zone the master is in is a master lair.
    Zone* pZone = pMonster->getZone();
    Assert(pZone != NULL);

    // Meaningless if it is not a master lair.
    if (!pZone->isMasterLair()) {
        return false;
    }

    HP_t currentHP = pMonster->getHP(ATTR_CURRENT);
    HP_t maxHP = pMonster->getHP(ATTR_MAX);

    // Not used when HP is above 70%.
    if (currentHP * 100 / maxHP > 70) {
        return false;
    }

    // No more summoning at more than 12 monsters.
    if (pZone->getMonsterManager()->getSize() > 12) {
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
//
// bool checkPossibleSummonMonsters(Monster* pMonster, Creature* pEnemy)
//
//----------------------------------------------------------------------
// Conditions for using the summon skill.
//----------------------------------------------------------------------
bool checkPossibleSummonMonsters(Monster* pMonster, Creature* pEnemy) {
    // cout << "checkMasterSummonTiming: ";

    Assert(pMonster != NULL);

    return pMonster->hasNextMonsterSummonInfo();
}

bool checkEnemyTileNotAcidSwamp(Monster* pMonster, Creature* pEnemy) {
    Zone* pZone = pEnemy->getZone();
    Assert(pZone != NULL);

    Tile& rTile = pZone->getTile(pEnemy->getX(), pEnemy->getY());
    if (rTile.getEffect(Effect::EFFECT_CLASS_ACID_SWAMP) == NULL)
        return true;

    return false;
}

bool checkEnemyOnAir(Monster* pMonster, Creature* pEnemy) {
    if (pEnemy == NULL)
        return false;
    if (pEnemy->getMoveMode() == Creature::MOVE_MODE_FLYING)
        return true;
    return false;
}

bool checkEnemyOnSafeZone(Monster* pMonster, Creature* pEnemy) {
    if (pEnemy == NULL)
        return false;
    return ((pEnemy->getZone()->getZoneLevel(pEnemy->getX(), pEnemy->getY()) & SAFE_ZONE) != 0);
}

bool checkCanAttackThrowingAxe(Monster* pMonster, Creature* pEnemy) {
    if (pEnemy == NULL)
        return false;
    Dir_t dir = getDirection(pMonster->getX(), pMonster->getY(), pEnemy->getX(), pEnemy->getY());
    ZoneCoord_t X = pMonster->getX() + dirMoveMask[dir].x * 7;
    ZoneCoord_t Y = pMonster->getY() + dirMoveMask[dir].y * 7;
    Distance_t distance = pMonster->getDistance(X, Y, pEnemy->getX(), pEnemy->getY());

    //	cout << "dir : " << (int)dir << ", distance : " << (int)distance << endl;

    if (distance > 2)
        return false;
    return true;
}
