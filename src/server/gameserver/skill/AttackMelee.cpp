//////////////////////////////////////////////////////////////////////////////
// Filename    : AttackMelee.cpp
// Written by  : elca@ewestsoft.com
// Description : Basic attack
//////////////////////////////////////////////////////////////////////////////

#include "AttackMelee.h"

#include "CrossCounter.h"
#include "EffectPoison.h"
#include "EffectProtectionFromPoison.h"
#include "GCAddEffect.h"
#include "GCAttackMeleeOK1.h"
#include "GCAttackMeleeOK2.h"
#include "GCAttackMeleeOK3.h"
#include "GCRemoveEffect.h"


//////////////////////////////////////////////////////////////////////////////
// Slayer object
//////////////////////////////////////////////////////////////////////////////
void AttackMelee::execute(Slayer* pSlayer, ObjectID_t TargetObjectID)

{
    Assert(pSlayer != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Assert(pPlayer != NULL);

        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // An NPC cannot be attacked.
        if (pTargetCreature == NULL // The zone returns NULL when the target is gone.
            || !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // Domain experience is earned only with a weapon in the right hand.
        bool bIncreaseDomainExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        // Get the basic attack skill slot.
        SkillSlot* pSkillSlot = pSlayer->hasSkill(SKILL_ATTACK_MELEE);
        Assert(pSkillSlot != NULL);

        GCAttackMeleeOK1 _GCAttackMeleeOK1;
        GCAttackMeleeOK2 _GCAttackMeleeOK2;
        GCAttackMeleeOK3 _GCAttackMeleeOK3;

        bool bCriticalHit = false;
        Damage_t Damage = computeDamage(pSlayer, pTargetCreature, 0, bCriticalHit);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, 1);
        bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature);
        bool bCanHit = canHit(pSlayer, pTargetCreature, SKILL_ATTACK_MELEE);
        bool bPK = verifyPK(pSlayer, pTargetCreature);

        if (bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK) {
            CheckCrossCounter(pSlayer, pTargetCreature, Damage);

            Exp_t Point = 1;

            // Apply the damage and reduce item durability.
            setDamage(pTargetCreature, Damage, pSlayer, SKILL_ATTACK_MELEE, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            computeAlignmentChange(pTargetCreature, Damage, pSlayer, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            decreaseDurability(pSlayer, pTargetCreature, NULL, &_GCAttackMeleeOK1, &_GCAttackMeleeOK2);

            // A critical hit knocks the target back.
            if (bCriticalHit) {
                knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
            }

            // Raise experience when the target is not a Slayer.
            if (!pTargetCreature->isSlayer()) {
                Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);

                if (pWeapon != NULL) {
                    if (bIncreaseDomainExp) {
                        if (pWeapon->getItemClass() == Item::ITEM_CLASS_BLADE) {
                            shareAttrExp(pSlayer, Damage, 8, 1, 1, _GCAttackMeleeOK1);
                            increaseDomainExp(pSlayer, SKILL_DOMAIN_BLADE, Point, _GCAttackMeleeOK1,
                                              pTargetCreature->getLevel());
                        } else if (pWeapon->getItemClass() == Item::ITEM_CLASS_SWORD) {
                            shareAttrExp(pSlayer, Damage, 8, 1, 1, _GCAttackMeleeOK1);
                            increaseDomainExp(pSlayer, SKILL_DOMAIN_SWORD, Point, _GCAttackMeleeOK1,
                                              pTargetCreature->getLevel());
                        } else if (pWeapon->getItemClass() == Item::ITEM_CLASS_CROSS) {
                            // Holding a cross raises Heal domain experience.
                            shareAttrExp(pSlayer, Damage, 1, 1, 8, _GCAttackMeleeOK1);
                            increaseDomainExp(pSlayer, SKILL_DOMAIN_HEAL, Point, _GCAttackMeleeOK1,
                                              pTargetCreature->getLevel());
                        } else if (pWeapon->getItemClass() == Item::ITEM_CLASS_MACE) {
                            // Holding a mace raises Enchant domain experience.
                            shareAttrExp(pSlayer, Damage, 1, 1, 8, _GCAttackMeleeOK1);
                            increaseDomainExp(pSlayer, SKILL_DOMAIN_ENCHANT, Point, _GCAttackMeleeOK1,
                                              pTargetCreature->getLevel());
                        } else {
                            // Using AttackMelee with another weapon class is not allowed.
                            Assert(false);
                        }
                    }
                } else {
                    // Attacking with no weapon raises only a little stat experience.
                    shareAttrExp(pSlayer, Damage, 1, 1, 1, _GCAttackMeleeOK1);
                }

                increaseAlignment(pSlayer, pTargetCreature, _GCAttackMeleeOK1);
            }

            _GCAttackMeleeOK3.setObjectID(pSlayer->getObjectID());
            _GCAttackMeleeOK3.setTargetObjectID(TargetObjectID);

            _GCAttackMeleeOK1.setObjectID(TargetObjectID);

            pPlayer->sendPacket(&_GCAttackMeleeOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                if (pTargetPlayer != NULL) {
                    _GCAttackMeleeOK2.setObjectID(pSlayer->getObjectID());
                    pTargetPlayer->sendPacket(&_GCAttackMeleeOK2);
                }
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pSlayer);

                // This part should be reworked to account for the newer code.
                // A master monster imposes no delay.
                if (!pMonster->isMaster()) {
                    Timeval NextTurn = pMonster->getNextTurn();
                    Timeval DelayTurn;
                    DelayTurn.tv_sec = 0;
                    DelayTurn.tv_usec = 200000;
                    Timeval NewTurn = NextTurn + DelayTurn;
                    pMonster->setNextTurn(NewTurn);
                }
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pSlayer);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCAttackMeleeOK3, cList);

            pSkillSlot->setRunTime();
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Vampire object
//////////////////////////////////////////////////////////////////////////////
void AttackMelee::execute(Vampire* pVampire, ObjectID_t TargetObjectID)

{
    Assert(pVampire != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of raising an exception.
        if (pTargetCreature == NULL || !canAttack(pVampire, pTargetCreature)) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }


        GCAttackMeleeOK1 _GCAttackMeleeOK1;
        GCAttackMeleeOK2 _GCAttackMeleeOK2;
        GCAttackMeleeOK3 _GCAttackMeleeOK3;

        // Get the skill slot.
        VampireSkillSlot* pSkillSlot = pVampire->hasSkill(SKILL_ATTACK_MELEE);
        Assert(pSkillSlot != NULL);

        bool bCriticalHit = false;

        Damage_t Damage = computeDamage(pVampire, pTargetCreature, 0, bCriticalHit);

        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, pTargetCreature, 1);
        bool bHitRoll = HitRoll::isSuccess(pVampire, pTargetCreature);
        bool bCanHit = canHit(pVampire, pTargetCreature, SKILL_ATTACK_MELEE);
        bool bPK = verifyPK(pVampire, pTargetCreature);

        if (bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK) {
            CheckCrossCounter(pVampire, pTargetCreature, Damage);

            // Apply the damage and reduce durability.
            setDamage(pTargetCreature, Damage, pVampire, SKILL_ATTACK_MELEE, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            computeAlignmentChange(pTargetCreature, Damage, pVampire, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            decreaseDurability(pVampire, pTargetCreature, NULL, &_GCAttackMeleeOK1, &_GCAttackMeleeOK2);

            // A critical hit knocks the target back.
            if (bCriticalHit) {
                knockbackCreature(pZone, pTargetCreature, pVampire->getX(), pVampire->getY());
            }

            increaseAlignment(pVampire, pTargetCreature, _GCAttackMeleeOK1);

            // Raise some experience when the target dies.
            if (pTargetCreature->isDead()) {
                int exp = computeCreatureExp(pTargetCreature, KILL_EXP);
                shareVampExp(pVampire, exp, _GCAttackMeleeOK1);
            }

            _GCAttackMeleeOK1.setObjectID(TargetObjectID);

            _GCAttackMeleeOK3.setObjectID(pVampire->getObjectID());
            _GCAttackMeleeOK3.setTargetObjectID(TargetObjectID);

            pPlayer->sendPacket(&_GCAttackMeleeOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                if (pTargetPlayer != NULL) {
                    _GCAttackMeleeOK2.setObjectID(pVampire->getObjectID());
                    pTargetPlayer->sendPacket(&_GCAttackMeleeOK2);
                }
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pVampire);

                // A master monster imposes no delay.
                if (!pMonster->isMaster()) {
                    Timeval NextTurn = pMonster->getNextTurn();
                    Timeval DelayTurn;
                    DelayTurn.tv_sec = 0;
                    DelayTurn.tv_usec = 200000;
                    Timeval NewTurn = NextTurn + DelayTurn;
                    pMonster->setNextTurn(NewTurn);
                }
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pVampire);
            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &_GCAttackMeleeOK3, cList);

            pSkillSlot->setRunTime();
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}


//////////////////////////////////////////////////////////////////////////////
// Ousters object
//////////////////////////////////////////////////////////////////////////////
void AttackMelee::execute(Ousters* pOusters, ObjectID_t TargetObjectID)

{
    Assert(pOusters != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of raising an exception.
        if (pTargetCreature == NULL || !canAttack(pOusters, pTargetCreature)) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        GCAttackMeleeOK1 _GCAttackMeleeOK1;
        GCAttackMeleeOK2 _GCAttackMeleeOK2;
        GCAttackMeleeOK3 _GCAttackMeleeOK3;

        // Get the skill slot.
        OustersSkillSlot* pSkillSlot = pOusters->hasSkill(SKILL_ATTACK_MELEE);
        Assert(pSkillSlot != NULL);

        bool bCriticalHit = false;

        Damage_t Damage = computeDamage(pOusters, pTargetCreature, 0, bCriticalHit);

        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, pTargetCreature, 1);
        bool bHitRoll = HitRoll::isSuccess(pOusters, pTargetCreature);
        bool bCanHit = canHit(pOusters, pTargetCreature, SKILL_ATTACK_MELEE);
        bool bPK = verifyPK(pOusters, pTargetCreature);

        if (bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK) {
            Damage += computeElementalCombatSkill(pOusters, pTargetCreature, _GCAttackMeleeOK1);
            CheckCrossCounter(pOusters, pTargetCreature, Damage);

            // Apply the damage and reduce durability.
            setDamage(pTargetCreature, Damage, pOusters, SKILL_ATTACK_MELEE, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            computeAlignmentChange(pTargetCreature, Damage, pOusters, &_GCAttackMeleeOK2, &_GCAttackMeleeOK1);
            decreaseDurability(pOusters, pTargetCreature, NULL, &_GCAttackMeleeOK1, &_GCAttackMeleeOK2);

            // A critical hit knocks the target back.
            if (bCriticalHit) {
                knockbackCreature(pZone, pTargetCreature, pOusters->getX(), pOusters->getY());
            }

            increaseAlignment(pOusters, pTargetCreature, _GCAttackMeleeOK1);

            // Raise some experience when the target dies.
            if (pTargetCreature->isDead()) {
                int exp = computeCreatureExp(pTargetCreature, 100, pOusters);
                shareOustersExp(pOusters, exp, _GCAttackMeleeOK1);
            }

            _GCAttackMeleeOK1.setObjectID(TargetObjectID);

            _GCAttackMeleeOK3.setObjectID(pOusters->getObjectID());
            _GCAttackMeleeOK3.setTargetObjectID(TargetObjectID);

            pPlayer->sendPacket(&_GCAttackMeleeOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                if (pTargetPlayer != NULL) {
                    _GCAttackMeleeOK2.setObjectID(pOusters->getObjectID());
                    pTargetPlayer->sendPacket(&_GCAttackMeleeOK2);
                }
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pOusters);

                // A master monster imposes no delay.
                if (!pMonster->isMaster()) {
                    Timeval NextTurn = pMonster->getNextTurn();
                    Timeval DelayTurn;
                    DelayTurn.tv_sec = 0;
                    DelayTurn.tv_usec = 200000;
                    Timeval NewTurn = NextTurn + DelayTurn;
                    pMonster->setNextTurn(NewTurn);
                }
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pOusters);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCAttackMeleeOK3, cList);

            pSkillSlot->setRunTime();
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Monster object
//////////////////////////////////////////////////////////////////////////////
void AttackMelee::execute(Monster* pMonster, Creature* pEnemy)

{
    Assert(pMonster != NULL);
    Assert(pEnemy != NULL);

    try {
        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
            return;
        }
        if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pMonster, true);
        }

        GCAttackMeleeOK2 _GCAttackMeleeOK2;
        GCAttackMeleeOK3 _GCAttackMeleeOK3;

        bool bCriticalHit = false;

        Damage_t Damage = computeDamage(pMonster, pEnemy, 0, bCriticalHit);

        bool bRangeCheck = verifyDistance(pMonster, pEnemy, pMonster->getMeleeRange());
        bool bHitRoll = HitRoll::isSuccess(pMonster, pEnemy);
        bool bCanHit = canHit(pMonster, pEnemy, SKILL_ATTACK_MELEE);

        if (bRangeCheck && bHitRoll && bCanHit) {
            CheckCrossCounter(pMonster, pEnemy, Damage);

            // Apply the damage and reduce durability.
            setDamage(pEnemy, Damage, pMonster, SKILL_ATTACK_MELEE, &_GCAttackMeleeOK2);
            decreaseDurability(pMonster, pEnemy, NULL, NULL, &_GCAttackMeleeOK2);

            // A critical hit knocks the target back.
            if (bCriticalHit) {
                knockbackCreature(pZone, pEnemy, pMonster->getX(), pMonster->getY());
            }

            _GCAttackMeleeOK3.setObjectID(pMonster->getObjectID());
            _GCAttackMeleeOK3.setTargetObjectID(pEnemy->getObjectID());

            if (pEnemy->isPC()) {
                Player* pTargetPlayer = pEnemy->getPlayer();
                Assert(pTargetPlayer != NULL);

                _GCAttackMeleeOK2.setObjectID(pMonster->getObjectID());

                pTargetPlayer->sendPacket(&_GCAttackMeleeOK2);
            } else if (pEnemy->isMonster()) {
                Monster* pTargetMonster = dynamic_cast<Monster*>(pEnemy);
                pTargetMonster->addEnemy(pMonster);
            }

            list<Creature*> cList;
            cList.push_back(pEnemy);

            cList.push_back(pMonster);
            pZone->broadcastPacket(pMonster->getX(), pMonster->getY(), &_GCAttackMeleeOK3, cList);
        } else {
            executeSkillFailNormal(pMonster, getSkillType(), pEnemy);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pMonster, getSkillType());
    }
}

AttackMelee g_AttackMelee;
