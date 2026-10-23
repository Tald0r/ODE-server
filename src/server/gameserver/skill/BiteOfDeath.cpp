//////////////////////////////////////////////////////////////////////////////
// Filename    : BiteOfDeath.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "BiteOfDeath.h"

#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToObjectOK6.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "RankBonus.h"
#include "Reflection.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void BiteOfDeath::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pSkillSlot,
                          CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // NPCs cannot be attacked.
        // It also fails if the target is immune to curses.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pVampire, pTargetCreature) || pTargetCreature->isNPC() ||
            !pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF) || pTargetCreature->isDead() ||
            pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;
        GCSkillToObjectOK4 _GCSkillToObjectOK4;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;
        GCSkillToObjectOK6 _GCSkillToObjectOK6;

        SkillType_t SkillType = getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bRangeCheck = verifyDistance(pVampire, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessBloodDrain(pVampire, pTargetCreature);
        bool bCanHit = canHit(pVampire, pTargetCreature, SkillType);
        bool bPK = verifyPK(pVampire, pTargetCreature);
        bool bMaster = false;

        if (pTargetCreature->isMonster()) {
            Monster* pTargetMonster = dynamic_cast<Monster*>(pTargetCreature);
            Assert(pTargetMonster != NULL);

            if (pTargetMonster->isMaster()) {
                bMaster = true;
            }
        }

        ZoneCoord_t targetX = pTargetCreature->getX();
        ZoneCoord_t targetY = pTargetCreature->getY();
        ZoneCoord_t myX = pVampire->getX();
        ZoneCoord_t myY = pVampire->getY();

        if (bManaCheck && bRangeCheck && bHitRoll && bCanHit && bPK && !bMaster) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToObjectOK1);

            bool bCanSeeCaster = canSee(pTargetCreature, pVampire);

            Exp_t Exp = computeCreatureExp(pTargetCreature, BLOODDRAIN_EXP + KILL_EXP);
            shareVampExp(pVampire, Exp, _GCSkillToObjectOK1);

            if (!pVampire->isFlag(Effect::EFFECT_CLASS_MEPHISTO)) {
                HP_t HealPoint = (Exp == 0 ? computeBloodDrainHealPoint(pTargetCreature, BLOODDRAIN_EXP) : Exp);
                HP_t CurrentHP = pVampire->getHP();
                HP_t MaxHP = pVampire->getHP(ATTR_MAX);
                HP_t NewHP = min((int)MaxHP, (int)CurrentHP + (int)HealPoint);

                // Handles the silver damage.
                Silver_t newSilverDamage = max(0, (int)pVampire->getSilverDamage() - (int)HealPoint);
                pVampire->saveSilverDamage(newSilverDamage);
                _GCSkillToObjectOK1.addShortData(MODIFY_SILVER_DAMAGE, newSilverDamage);

                // Sets the Vampire HP.
                pVampire->setHP(NewHP);

                GCStatusCurrentHP gcStatusCurrentHP;
                gcStatusCurrentHP.setObjectID(pVampire->getObjectID());
                gcStatusCurrentHP.setCurrentHP(NewHP);
                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcStatusCurrentHP, pVampire);

                _GCSkillToObjectOK1.addShortData(MODIFY_CURRENT_HP, NewHP);
            }


            // Kills the target.
            if (pTargetCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pTargetCreature);
                pSlayer->setHP(0);
            } else if (pTargetCreature->isVampire()) {
                Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetCreature);
                pTargetVampire->setHP(0);
            } else if (pTargetCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pTargetCreature);
                pOusters->setHP(0);
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                HP_t decreaseHP = pMonster->getHP();
                pMonster->setHP(0);

                increaseFame(pVampire, decreaseHP);
                _GCSkillToObjectOK1.addLongData(MODIFY_FAME, pVampire->getFame());
            } else
                Assert(false);

            affectKillCount(pVampire, pTargetCreature);

            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);

            _GCSkillToObjectOK3.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK5.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK6.setXY(myX, myY);
            _GCSkillToObjectOK6.setSkillType(SkillType);

            if (bCanSeeCaster) // 10 is a stopgap value.
            {
                computeAlignmentChange(pTargetCreature, 10, pVampire, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
            } else // 10 is a stopgap value.
            {
                computeAlignmentChange(pTargetCreature, 10, pVampire, &_GCSkillToObjectOK6, &_GCSkillToObjectOK1);
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pVampire);
            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCSkillToObjectOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                if (pTargetPlayer == NULL) {
                    return;
                }

                if (bCanSeeCaster)
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
                else
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK6);
            } else if (pTargetCreature->isMonster()) {
                Monster* pTargetMonster = dynamic_cast<Monster*>(pTargetCreature);
                pTargetMonster->addEnemy(pVampire);
            }
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

BiteOfDeath g_BiteOfDeath;
