//////////////////////////////////////////////////////////////////////////////
// Filename    : SetAfire.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SetAfire.h"

#include "CrossCounter.h"
#include "EffectSetAfire.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK5.h"
#include "GameContext.h"
#include "HitRoll.h"
#include "RankBonus.h"
#include "SkillUtil.h"
#include "Vampire.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void SetAfire::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // An NPC cannot be attacked.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pVampire, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        Range_t Range = 2 + pVampire->getSTR() / 20 + pVampire->getDEX() / 220 + pVampire->getINT() / 440;
        Range = min((Range_t)6, Range);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, pTargetCreature, Range);
        bool bHitRoll = HitRoll::isSuccess(pVampire, pTargetCreature);
        bool bCanHit = canHit(pVampire, pTargetCreature, getSkillType());
        bool bPK = verifyPK(pVampire, pTargetCreature);
        bool bFastMove = false;

        if (getDistance(pVampire->getX(), pVampire->getY(), pTargetCreature->getX(), pTargetCreature->getY()) > 1) {
            bFastMove = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK &&
            (!bFastMove || pZone->moveFastPC(pVampire, pVampire->getX(), pVampire->getY(), pTargetCreature->getX(),
                                             pTargetCreature->getY(), getSkillType()))) {
            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);
            Damage_t Damage = output.Damage;
            bool bCriticalHit = false;
            Damage += computeDamage(pVampire, pTargetCreature, 0, bCriticalHit);

            if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_TIGER_NAIL)) {
                RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_TIGER_NAIL);
                Assert(pRankBonus != NULL);

                Damage += getPercentValue(Damage, pRankBonus->getPoint());
            }

            CheckCrossCounter(pVampire, pTargetCreature, Damage);

            // Consume the mana.
            decreaseMana(pVampire, RequiredMP, _GCSkillToObjectOK1);

            // Deal the damage and reduce item durability.

            // A critical hit knocks the target back.

            // Experience goes up if this attack killed the target.
            EffectSetAfire* pEffect = new EffectSetAfire(pTargetCreature);
            pEffect->setDamage(Damage);
            pEffect->setTick(3);
            pEffect->setNextTime(3);
            pEffect->setTimes(2);
            pEffect->setCasterOID(pVampire->getObjectID());
            pTargetCreature->addEffect(pEffect);
            //			}

            increaseAlignment(pVampire, pTargetCreature, _GCSkillToObjectOK1);

            // Send the packet.
            _GCSkillToObjectOK1.setSkillType(getSkillType());
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK2.setSkillType(getSkillType());
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK5.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setSkillType(getSkillType());

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            Player* pTargetPlayer = NULL;

            if (pTargetCreature->isPC()) {
                pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pVampire);
            }

            list<Creature*> cList;
            cList.push_back(pVampire);
            cList.push_back(pTargetCreature);

            cList = pZone->broadcastSkillPacket(pVampire->getX(), pVampire->getY(), pTargetCreature->getX(),
                                                pTargetCreature->getY(), &_GCSkillToObjectOK5, cList);

            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

SetAfire g_SetAfire;
