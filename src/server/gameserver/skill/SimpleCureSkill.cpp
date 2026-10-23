//////////////////////////////////////////////////////////////////////////////
// Filename    : SimpleCureSkill.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SimpleCureSkill.h"

#include "EffectAftermath.h"
#include "EffectBloodDrain.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

SimpleCureSkill g_SimpleCureSkill;

//////////////////////////////////////////////////////////////////////////////
// class SimpleCureSkill member methods
//////////////////////////////////////////////////////////////////////////////

void SimpleCureSkill::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, const SIMPLE_SKILL_INPUT& param,
                              SIMPLE_SKILL_OUTPUT& result, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bHPCheck = false;

        EffectBloodDrain* pEffectBloodDrain = NULL;

        if (pSlayer->getHP(ATTR_CURRENT) < pSlayer->getHP(ATTR_MAX)) {
            bHPCheck = true;
        }
        if (pSlayer->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
            Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
            pEffectBloodDrain = dynamic_cast<EffectBloodDrain*>(pEffect);
            Assert(pEffectBloodDrain != NULL);

            if (pEffectBloodDrain->getLevel() < param.Level)
                bHPCheck = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bHPCheck && pSlayer->isAlive()) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            uint HealPoint = param.SkillDamage;

            // Clear the blood drain state if the target is under it.
            if (pEffectBloodDrain != NULL && pEffectBloodDrain->getLevel() < param.Level) {
                // Attach an aftermath effect to prevent blood drain farming.
                if (pSlayer->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                    Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                    EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                } else {
                    EffectAftermath* pEffectAftermath = new EffectAftermath(pSlayer);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                    pSlayer->addEffect(pEffectAftermath);
                    pSlayer->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                    pEffectAftermath->create(pSlayer->getName());
                }

                pEffectBloodDrain->destroy(pSlayer->getName());
                pSlayer->deleteEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);

                SLAYER_RECORD prev;
                pSlayer->getSlayerRecord(prev);
                pSlayer->initAllStat();
                pSlayer->sendRealWearingInfo();
                pSlayer->addModifyInfo(prev, _GCSkillToSelfOK1);

                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(pSlayer->getObjectID());
                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);
                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);
            }

            // Set the HP.
            HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
            HP_t MaxHP = pSlayer->getHP(ATTR_MAX);

            // Compute the actual amount healed.
            int RealHealPoint = 0;
            if (CurrentHP + HealPoint <= MaxHP) {
                RealHealPoint = max((unsigned int)0, HealPoint);
            } else {
                RealHealPoint = max(0, MaxHP - CurrentHP);
            }

            // Raises experience.
            shareAttrExp(pSlayer, RealHealPoint, param.STRMultiplier, param.DEXMultiplier, param.INTMultiplier,
                         _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            CurrentHP = min((int)MaxHP, (int)(CurrentHP + HealPoint));
            pSlayer->setHP(CurrentHP, ATTR_CURRENT);

            // Broadcast the HP.
            GCStatusCurrentHP gcStatusCurrentHP;
            gcStatusCurrentHP.setObjectID(pSlayer->getObjectID());
            gcStatusCurrentHP.setCurrentHP(CurrentHP);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcStatusCurrentHP);

            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCSkillToSelfOK1.setSkillType(param.SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(param.SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(myX, myY, &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(param.Delay);

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}

void SimpleCureSkill::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot,
                              const SIMPLE_SKILL_INPUT& param, SIMPLE_SKILL_OUTPUT& result, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // Only a Slayer can be cured.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || pTargetCreature->isSlayer() == false) {
            executeSkillFailException(pSlayer, param.SkillType);
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;
        GCSkillToObjectOK4 _GCSkillToObjectOK4;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        bool bHPCheck = false;

        // The target must have lost HP or be under blood drain.
        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetCreature);
        Assert(pTargetSlayer != NULL);

        EffectBloodDrain* pEffectBloodDrain = NULL;

        if (pTargetSlayer->getHP(ATTR_CURRENT) < pTargetSlayer->getHP(ATTR_MAX)) {
            bHPCheck = true;
        }
        if (pTargetSlayer->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
            Effect* pEffect = pTargetSlayer->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
            pEffectBloodDrain = dynamic_cast<EffectBloodDrain*>(pEffect);
            Assert(pEffectBloodDrain != NULL);

            if (pEffectBloodDrain->getLevel() < param.Level)
                bHPCheck = true;
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);

        if (bHPCheck && bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && pTargetCreature->isAlive()) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            uint HealPoint = param.SkillDamage;

            // Clear the blood drain state if the target is under it.
            if (pEffectBloodDrain != NULL && pEffectBloodDrain->getLevel() < param.Level) {
                // Attach an aftermath effect to prevent blood drain farming.
                if (pTargetSlayer->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                    Effect* pEffect = pTargetSlayer->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                    EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                } else {
                    EffectAftermath* pEffectAftermath = new EffectAftermath(pTargetSlayer);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                    pTargetSlayer->addEffect(pEffectAftermath);
                    pTargetSlayer->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                    pEffectAftermath->create(pTargetSlayer->getName());
                }


                pEffectBloodDrain->destroy(pTargetSlayer->getName());
                pTargetSlayer->deleteEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);

                SLAYER_RECORD prev;
                pTargetSlayer->getSlayerRecord(prev);
                pTargetSlayer->initAllStat();
                pTargetSlayer->sendRealWearingInfo();
                pTargetSlayer->addModifyInfo(prev, _GCSkillToObjectOK2);

                GCRemoveEffect gcRemoveEffect;
                gcRemoveEffect.setObjectID(pTargetSlayer->getObjectID());
                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);
                pZone->broadcastPacket(pTargetSlayer->getX(), pTargetSlayer->getY(), &gcRemoveEffect);
            }

            // Heal the other creature.
            HP_t CurrentHP = pTargetSlayer->getHP(ATTR_CURRENT);
            HP_t MaxHP = pTargetSlayer->getHP(ATTR_MAX);

            // Compute the actual amount healed.

            // Raises experience.
            shareAttrExp(pSlayer, HealPoint, param.STRMultiplier, param.DEXMultiplier, param.INTMultiplier,
                         _GCSkillToObjectOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);

            CurrentHP = min((int)(MaxHP), (int)(CurrentHP + HealPoint));
            pTargetSlayer->setHP(CurrentHP, ATTR_CURRENT);

            // Broadcast the HP now that the target is healed.
            GCStatusCurrentHP gcStatusCurrentHP;
            gcStatusCurrentHP.setObjectID(TargetObjectID);
            gcStatusCurrentHP.setCurrentHP(CurrentHP);
            pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcStatusCurrentHP);

            _GCSkillToObjectOK2.addShortData(MODIFY_CURRENT_HP, CurrentHP);

            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();
            ZoneCoord_t targetX = pTargetCreature->getX();
            ZoneCoord_t targetY = pTargetCreature->getY();

            _GCSkillToObjectOK1.setSkillType(param.SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(param.SkillType);
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK3.setSkillType(param.SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(param.SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setSkillType(param.SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            }

            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList.push_back(pTargetCreature);

            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCSkillToObjectOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            pSkillSlot->setRunTime(param.Delay);

            result.bSuccess = true;
        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}
