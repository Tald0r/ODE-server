//////////////////////////////////////////////////////////////////////////////
// Filename    : MoleShot.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MoleShot.h"

#include <list>

#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK3.h"
#include "GCSkillToTileOK4.h"
#include "GCSkillToTileOK5.h"
#include "GameContext.h"
#include "ItemUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor - initializes the masks.
//////////////////////////////////////////////////////////////////////////////
MoleShot::MoleShot() {
    // The SG mask does not depend on the direction.
    m_pSGMask[0].set(-1, -1);
    m_pSGMask[1].set(0, -1);
    m_pSGMask[2].set(1, -1);
    m_pSGMask[3].set(-1, 0);
    m_pSGMask[4].set(0, 0);
    m_pSGMask[5].set(1, 0);
    m_pSGMask[6].set(-1, 1);
    m_pSGMask[7].set(0, 1);
    m_pSGMask[8].set(1, 1);

    // Initialize the AR mask.
    // It resembles SwordWave, but SwordWave checks the tiles around the caster
    // with its mask, while MoleShot lays the mask around the target
    // instead.
    //
    // (-1,-1)(0,-1)(1,-1)
    // (-1, 0)(0, 0)(1, 0)
    // (-1, 1)(0, 1)(1, 1)
    //
    // The mask is easiest to read by asking which tiles are hit
    // for each direction.

    m_pARMask[LEFT][0].set(0, -1);
    m_pARMask[LEFT][1].set(0, 0);
    m_pARMask[LEFT][2].set(0, 1);

    m_pARMask[RIGHT][0].set(0, -1);
    m_pARMask[RIGHT][1].set(0, 0);
    m_pARMask[RIGHT][2].set(0, 1);

    m_pARMask[UP][0].set(-1, 0);
    m_pARMask[UP][1].set(0, 0);
    m_pARMask[UP][2].set(1, 0);

    m_pARMask[DOWN][0].set(-1, 0);
    m_pARMask[DOWN][1].set(0, 0);
    m_pARMask[DOWN][2].set(1, 0);

    m_pARMask[LEFTUP][0].set(1, -1);
    m_pARMask[LEFTUP][1].set(0, 0);
    m_pARMask[LEFTUP][2].set(-1, 1);

    m_pARMask[RIGHTDOWN][0].set(1, -1);
    m_pARMask[RIGHTDOWN][1].set(0, 0);
    m_pARMask[RIGHTDOWN][2].set(-1, 1);

    m_pARMask[LEFTDOWN][0].set(-1, -1);
    m_pARMask[LEFTDOWN][1].set(0, 0);
    m_pARMask[LEFTDOWN][2].set(1, 1);

    m_pARMask[RIGHTUP][0].set(-1, -1);
    m_pARMask[RIGHTUP][1].set(0, 0);
    m_pARMask[RIGHTUP][2].set(1, 1);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void MoleShot::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // With no item, or with a weapon that is not a gun,
        // this skill cannot be used.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || !isArmsWeapon(pItem))
        // An SR is allowed as well.
        //	|| pItem->getItemClass() == Item::ITEM_CLASS_SR)
        {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        Item::ItemClass IClass = pItem->getItemClass();
        if (IClass == Item::ITEM_CLASS_SG) {
            SGexecute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
        } else if (IClass == Item::ITEM_CLASS_AR ||
                   IClass == Item::ITEM_CLASS_SMG
                   // An SR is allowed as well.
                   || IClass == Item::ITEM_CLASS_SR) {
            ARSMGexecute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void MoleShot::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        // With no item, or with a weapon that is not a gun,
        // this skill cannot be used.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || !isArmsWeapon(pItem))
        // An SR is allowed as well.
        //|| pItem->getItemClass() == Item::ITEM_CLASS_SR)
        {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        Item::ItemClass IClass = pItem->getItemClass();
        if (IClass == Item::ITEM_CLASS_SG) {
            SGexecute(pSlayer, X, Y, pSkillSlot, CEffectID);
        } else if (IClass == Item::ITEM_CLASS_AR ||
                   IClass == Item::ITEM_CLASS_SMG
                   // An SR is allowed as well.
                   || IClass == Item::ITEM_CLASS_SR) {
            ARSMGexecute(pSlayer, X, Y, pSkillSlot, CEffectID);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer tile handler - when holding an SG
//////////////////////////////////////////////////////////////////////////////
void MoleShot::SGexecute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(getSkillType());
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        Level_t SkillLevel = pSkillSlot->getExpLevel();

        Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        Assert(pWeapon != NULL);

        bool bIncreaseExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pWeapon->getRange());
        bool bBulletCheck = (getRemainBullet(pWeapon) > 0) ? true : false;

        // The bullet count drops regardless of the other checks.
        Bullet_t RemainBullet = 0;
        if (bBulletCheck) {
            decreaseBullet(pWeapon);
            // The weapon is not saved on every shot.
            RemainBullet = getRemainBullet(pWeapon);
        }

        // Work out the damage, the to-hit bonus, and the coordinates and direction.
        int ToHitBonus = 0;
        int DamageBonus = 0;
        int ToHitPenalty = 0;
        int DamagePenalty = 0;
        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();
        Dir_t dir = calcDirection(myX, myY, X, Y);
        bool bHit = false;   // Set once at least one target is hit
        Damage_t Damage = 0; // Damage dealt to the last target

        // For an SG the splash radius starts at 4 and grows with the skill's level.
        int Splash = 3 + pSkillSlot->getExpLevel() / 10 + 1;

        if (bManaCheck && bTimeCheck && bRangeCheck && bBulletCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // output.ToHit is negative, so the percentage comes back negative too.
            ToHitPenalty = getPercentValue(pSlayer->getToHit(), output.ToHit);

            list<Creature*> cList;
            list<Creature*> creatureList;
            getSplashVictims(pZone, X, Y, Creature::CREATURE_CLASS_MAX, creatureList, Splash);

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            list<Creature*>::iterator itr = creatureList.begin();
            for (; itr != creatureList.end(); itr++) {
                Creature* pTargetCreature = (*itr);
                Assert(pTargetCreature != NULL);

                ToHitBonus =
                    computeArmsWeaponToHitBonus(pWeapon, myX, myY, pTargetCreature->getX(), pTargetCreature->getY());
                DamageBonus =
                    computeArmsWeaponDamageBonus(pWeapon, myX, myY, pTargetCreature->getX(), pTargetCreature->getY());

                bool bInvokerCheck = (pTargetCreature->getObjectID() == pSlayer->getObjectID()) ? true : false;
                bool bRaceCheck = pTargetCreature->isSlayer() || pTargetCreature->isNPC();
                bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, ToHitPenalty + ToHitBonus);
                bool bPK = verifyPK(pSlayer, pTargetCreature);
                bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);

                if (!canAttack(pSlayer, pTargetCreature) || pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                    bHitRoll = false;
                }

                if (!bInvokerCheck && !bRaceCheck && bHitRoll && bPK && bZoneLevelCheck) {
                    bool bCriticalHit = false;

                    // Compute the damage and apply the penalty.
                    // The bonus can be negative because of the multishot penalty.
                    Damage = computeDamage(pSlayer, pTargetCreature, SkillLevel / 5, bCriticalHit);
                    DamagePenalty = getPercentValue(Damage, output.Damage);
                    Damage = max(0, Damage + DamagePenalty + DamageBonus);

                    // Everything but the main target takes splash damage,
                    // which is 50% of the normal damage.
                    if (pTargetCreature->getX() != X || pTargetCreature->getY() != Y) {
                        Damage = Damage / 2;
                    }

                    // Unlike SwordWave, no cross counter check is made.
                    ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                    cList.push_back(pTargetCreature);
                    _GCSkillToTileOK1.addCListElement(targetObjectID);
                    _GCSkillToTileOK2.addCListElement(targetObjectID);
                    _GCSkillToTileOK5.addCListElement(targetObjectID);

                    setDamage(pTargetCreature, Damage, pSlayer, getSkillType(), NULL, &_GCSkillToTileOK1);
                    computeAlignmentChange(pTargetCreature, Damage, pSlayer, NULL, &_GCSkillToTileOK1);

                    increaseAlignment(pSlayer, pTargetCreature, _GCSkillToTileOK1);

                    // A critical hit knocks the target back.
                    if (bCriticalHit) {
                        knockbackCreature(pZone, pTargetCreature, myX, myY);
                    }

                    // Only a target that is not a Slayer counts as a hit.
                    if (!pTargetCreature->isSlayer()) {
                        bHit = true;

                        if (maxEnemyLevel < pTargetCreature->getLevel())
                            maxEnemyLevel = pTargetCreature->getLevel();
                        EnemyNum++;
                    }
                }
            }

            if (bHit) {
                if (bIncreaseExp) {
                    shareAttrExp(pSlayer, Damage, 1, 8, 1, _GCSkillToTileOK1);
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1, maxEnemyLevel,
                                      EnemyNum);
                    increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);
                }
            }

            _GCSkillToTileOK1.addShortData(MODIFY_BULLET, RemainBullet);

            // The bullet count was already decreased and read; now drop the durability.
            decreaseDurability(pSlayer, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(getSkillType());
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(getSkillType());
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(getSkillType());
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(getSkillType());
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(0);
            _GCSkillToTileOK4.setRange(dir);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            // Send the packet to everyone affected by this skill.
            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                Assert(pTargetCreature != NULL);

                if (pTargetCreature->isPC()) {
                    _GCSkillToTileOK2.clearList();

                    HP_t targetHP = 0;
                    if (pTargetCreature->isSlayer()) {
                        targetHP = (dynamic_cast<Slayer*>(pTargetCreature))->getHP();
                    } else if (pTargetCreature->isVampire()) {
                        targetHP = (dynamic_cast<Vampire*>(pTargetCreature))->getHP();
                    } else if (pTargetCreature->isOusters()) {
                        targetHP = (dynamic_cast<Ousters*>(pTargetCreature))->getHP();
                    }

                    _GCSkillToTileOK2.addShortData(MODIFY_CURRENT_HP, targetHP);

                    // Wear down the target's item durability.
                    decreaseDurability(NULL, pTargetCreature, pSkillInfo, NULL, &_GCSkillToTileOK2);

                    // Send the packet.
                    Player* pPlayer = pTargetCreature->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&_GCSkillToTileOK2);
                } else if (pTargetCreature->isMonster()) {
                    // The monster takes the caster as an enemy.
                    Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                    pMonster->addEnemy(pSlayer);
                }
            }

            cList.push_back(pSlayer);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormalWithGun(pSlayer, getSkillType(), NULL, RemainBullet);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer tile handler - when holding an AR or an SMG
//////////////////////////////////////////////////////////////////////////////
void MoleShot::ARSMGexecute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK3 _GCSkillToTileOK3;
        GCSkillToTileOK4 _GCSkillToTileOK4;
        GCSkillToTileOK5 _GCSkillToTileOK5;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(getSkillType());
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        Level_t SkillLevel = pSkillSlot->getExpLevel();

        Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        Assert(pWeapon != NULL);

        bool bIncreaseExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pWeapon->getRange());
        bool bBulletCheck = (getRemainBullet(pWeapon) > 0) ? true : false;

        // The bullet count always drops.
        Bullet_t RemainBullet = 0;
        if (bBulletCheck) {
            decreaseBullet(pWeapon);
            // The weapon is not saved on every shot.
            RemainBullet = getRemainBullet(pWeapon);
        }

        // Work out the damage, the to-hit bonus, and the coordinates and direction.
        int ToHitBonus = 0;
        int DamageBonus = 0;
        int ToHitPenalty = 0;
        int DamagePenalty = 0;
        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();
        Dir_t dir = calcDirection(myX, myY, X, Y);
        bool bHit = false;   // Set once at least one target is hit
        Damage_t Damage = 0; // Damage dealt to the last target

        // For an AR or SMG the splash radius starts at 2 and grows with the skill's level.
        int Splash = 1 + pSkillSlot->getExpLevel() / 30 + 1;

        if (bManaCheck && bTimeCheck && bRangeCheck && bBulletCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToTileOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // The value comes back negative.
            ToHitPenalty = getPercentValue(pSlayer->getToHit(), output.ToHit);

            list<Creature*> cList;
            list<Creature*> creatureList;
            getSplashVictims(pZone, X, Y, Creature::CREATURE_CLASS_MAX, creatureList, Splash);

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            list<Creature*>::iterator itr = creatureList.begin();
            for (; itr != creatureList.end(); itr++) {
                Creature* pTargetCreature = (*itr);
                Assert(pTargetCreature != NULL);

                ToHitBonus =
                    computeArmsWeaponToHitBonus(pWeapon, myX, myY, pTargetCreature->getX(), pTargetCreature->getY());
                DamageBonus =
                    computeArmsWeaponDamageBonus(pWeapon, myX, myY, pTargetCreature->getX(), pTargetCreature->getY());

                bool bInvokerCheck = (pTargetCreature->getObjectID() == pSlayer->getObjectID()) ? true : false;
                bool bRaceCheck = pTargetCreature->isSlayer() || pTargetCreature->isNPC();
                bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, ToHitPenalty + ToHitBonus);
                bool bPK = verifyPK(pSlayer, pTargetCreature);
                bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature);

                if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE) ||
                    pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                    bHitRoll = false;
                }

                if (!bInvokerCheck && !bRaceCheck && bHitRoll && bPK && bZoneLevelCheck) {
                    bool bCriticalHit = false;

                    // Compute the damage and apply the penalty.
                    // The bonus can be negative because of the multishot penalty.
                    Damage = computeDamage(pSlayer, pTargetCreature, SkillLevel / 5, bCriticalHit);
                    DamagePenalty = getPercentValue(Damage, output.Damage);
                    Damage = max(0, Damage + DamagePenalty + DamageBonus);

                    // Everything but the main target takes splash damage,
                    // which is 50% of the normal damage.
                    if (pTargetCreature->getX() != X || pTargetCreature->getY() != Y) {
                        Damage = Damage / 2;
                    }

                    // Unlike SwordWave, no cross counter check is made.
                    ObjectID_t targetObjectID = pTargetCreature->getObjectID();
                    cList.push_back(pTargetCreature);
                    _GCSkillToTileOK1.addCListElement(targetObjectID);
                    _GCSkillToTileOK2.addCListElement(targetObjectID);
                    _GCSkillToTileOK5.addCListElement(targetObjectID);

                    setDamage(pTargetCreature, Damage, pSlayer, getSkillType(), NULL, &_GCSkillToTileOK1);
                    computeAlignmentChange(pTargetCreature, Damage, pSlayer, NULL, &_GCSkillToTileOK1);

                    // A critical hit knocks the target back.
                    if (bCriticalHit) {
                        knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
                    }

                    // Only a target that is not a Slayer counts as a hit.
                    if (!pTargetCreature->isSlayer()) {
                        bHit = true;
                        if (maxEnemyLevel < pTargetCreature->getLevel())
                            maxEnemyLevel = pTargetCreature->getLevel();
                        EnemyNum++;
                    }
                }
            }

            if (bHit) {
                if (bIncreaseExp) {
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToTileOK1, maxEnemyLevel,
                                      EnemyNum);
                    shareAttrExp(pSlayer, Damage, 1, 8, 1, _GCSkillToTileOK1);
                }
                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToTileOK1);
            }

            _GCSkillToTileOK1.addShortData(MODIFY_BULLET, RemainBullet);

            decreaseDurability(pSlayer, NULL, pSkillInfo, &_GCSkillToTileOK1, NULL);

            _GCSkillToTileOK1.setSkillType(getSkillType());
            _GCSkillToTileOK1.setCEffectID(CEffectID);
            _GCSkillToTileOK1.setX(X);
            _GCSkillToTileOK1.setY(Y);
            _GCSkillToTileOK1.setRange(dir);
            _GCSkillToTileOK1.setDuration(0);

            _GCSkillToTileOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK2.setSkillType(getSkillType());
            _GCSkillToTileOK2.setX(X);
            _GCSkillToTileOK2.setY(Y);
            _GCSkillToTileOK2.setRange(dir);
            _GCSkillToTileOK2.setDuration(0);

            _GCSkillToTileOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK3.setSkillType(getSkillType());
            _GCSkillToTileOK3.setX(X);
            _GCSkillToTileOK3.setY(Y);

            _GCSkillToTileOK4.setSkillType(getSkillType());
            _GCSkillToTileOK4.setX(X);
            _GCSkillToTileOK4.setY(Y);
            _GCSkillToTileOK4.setDuration(0);
            _GCSkillToTileOK4.setRange(dir);

            _GCSkillToTileOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setX(X);
            _GCSkillToTileOK5.setY(Y);
            _GCSkillToTileOK5.setRange(dir);
            _GCSkillToTileOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            // Send the packet to everyone affected by this skill.
            for (list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); itr++) {
                Creature* pTargetCreature = *itr;
                Assert(pTargetCreature != NULL);

                if (pTargetCreature->isPC()) {
                    _GCSkillToTileOK2.clearList();

                    HP_t targetHP = 0;
                    if (pTargetCreature->isSlayer()) {
                        targetHP = (dynamic_cast<Slayer*>(pTargetCreature))->getHP();
                    } else if (pTargetCreature->isVampire()) {
                        targetHP = (dynamic_cast<Vampire*>(pTargetCreature))->getHP();
                    }

                    _GCSkillToTileOK2.addShortData(MODIFY_CURRENT_HP, targetHP);

                    // Wear down the target's item durability.
                    decreaseDurability(NULL, pTargetCreature, pSkillInfo, NULL, &_GCSkillToTileOK2);

                    // Send the packet.
                    Player* pPlayer = pTargetCreature->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&_GCSkillToTileOK2);
                } else if (pTargetCreature->isMonster()) {
                    // The monster takes the caster as an enemy.
                    Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                    pMonster->addEnemy(pSlayer);
                }
            }

            cList.push_back(pSlayer);

            cList = pZone->broadcastSkillPacket(myX, myY, X, Y, &_GCSkillToTileOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToTileOK3, cList);
            pZone->broadcastPacket(X, Y, &_GCSkillToTileOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormalWithGun(pSlayer, getSkillType(), NULL, RemainBullet);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

MoleShot g_MoleShot;
