//////////////////////////////////////////////////////////////////////////////
// Filename    : XRL Missile.cpp
// Written by  : rallser
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "XRLMissile.h"

#include "EffectXRLMissile.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "ItemUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void XRLMissile::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pSlayer, pTargetCreature)) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // Computes the strength and the duration of the effect.
        SkillInput input(pSlayer, pSkillSlot);
        SkillOutput output;
        input.Range = getDistance(pSlayer->getX(), pSlayer->getY(), pTargetCreature->getX(), pTargetCreature->getY());
        computeOutput(input, output);

        Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pWeapon == NULL || isArmsWeapon(pWeapon) == false) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        // Computes the penalty value.
        int ToHitPenalty = getPercentValue(pSlayer->getToHit(), output.ToHit);

        bool bIncreaseDomainExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, ToHitPenalty);
        bool bEffected = false; // pTargetCreature->isFlag(Effect::EFFECT_CLASS_XRL_Missile);
        bool bPK = verifyPK(pSlayer, pTargetCreature);
        bool bBulletCheck = (getRemainBullet(pWeapon) > 0) ? true : false;


        printf("remp : %d, bMcheck : %d, bTimeCheck : %d, bRangeCheck : %d, bHitroll : %d, bEffect : %d, bPK : %d\n",
               RequiredMP, bManaCheck, bTimeCheck, bRangeCheck, bHitRoll, bEffected, bPK);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bPK && bBulletCheck) {
            printf("check OK\n");
            // Reduces mana.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            decreaseBullet(pWeapon);
            // The weapon is not saved on every shot.
            getRemainBullet(pWeapon); // Logs and throws on an invalid weapon class.


            if (!pTargetCreature->isSlayer()) {
                // Raises experience.
                //
                if (bIncreaseDomainExp) {
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1,
                                      pTargetCreature->getLevel());
                }

                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);
            }

            // Creates the effect and attaches it.
            EffectXRLMissile* pEffect = new EffectXRLMissile(pTargetCreature);
            Assert(pEffect != NULL);
            pEffect->setNextTime(output.Duration);
            pEffect->setPoint(output.Damage);
            pEffect->setUserObjectID(pSlayer->getObjectID());

            pTargetCreature->addEffect(pEffect);
            pTargetCreature->setFlag(Effect::EFFECT_CLASS_XRL_Missile);

            // Prepare the packet and send it.
            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(output.Duration);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(output.Duration);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(output.Duration);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pSlayer != pTargetCreature && pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                if (pMonster != NULL)
                    pMonster->addEnemy(pSlayer);
            }

            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList.push_back(pTargetCreature);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToObjectOK5, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            printf("nonono~1\n");
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        printf("skill exception!!~1\n");
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

XRLMissile g_XRLMissile;
