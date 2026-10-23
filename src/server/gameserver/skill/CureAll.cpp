//////////////////////////////////////////////////////////////////////////////
// Filename    : CureAll.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CureAll.h"

#include "CurePoison.h"
#include "EffectAftermath.h"
#include "EffectBloodDrain.h"
#include "EffectBlunting.h"
#include "EffectDarkBluePoison.h"
#include "EffectDoom.h"
#include "EffectGreenStalker.h"
#include "EffectParalyze.h"
#include "EffectPoison.h"
#include "EffectSeduction.h"
#include "EffectYellowPoisonToCreature.h"
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
#include "SimpleCureSkill.h"

const uint CureAllBloodDrainLevel = 75;

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void CureAll::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    input.TargetType = SkillInput::TARGET_OTHER;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.Level = CureAllBloodDrainLevel;


    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        // Only a Slayer can be cured.
        if (pTargetCreature == NULL || pTargetCreature->isSlayer() == false) {
            executeSkillFailException(pSlayer, param.SkillType);
            return;
        }

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        Level_t SkillLevel = pSkillSlot->getExpLevel();

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;
        GCSkillToObjectOK4 _GCSkillToObjectOK4;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        bool bHPCheck = false;

        // The target must have lost HP or be under blood drain.
        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetCreature);
        Assert(pTargetSlayer != NULL);

        EffectBloodDrain* pEffectBloodDrain = NULL;

        bool bEffected = false; // Set when any curable effect is present.

        if (pTargetSlayer->getHP(ATTR_CURRENT) < pTargetSlayer->getHP(ATTR_MAX)) {
            bHPCheck = true;
        }
        if (pTargetSlayer->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
            Effect* pEffect = pTargetSlayer->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
            pEffectBloodDrain = dynamic_cast<EffectBloodDrain*>(pEffect);
            Assert(pEffectBloodDrain != NULL);

            if (pEffectBloodDrain->getLevel() < CureAllBloodDrainLevel)
                bHPCheck = true;
            bEffected = true;
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);

        bool bDoom = false;
        bool bBlunting = false;
        bool bParalyze = false;
        bool bSeduction = false;

        EffectDoom* pEffectDoom = NULL;
        EffectBlunting* pEffectBlunting = NULL;
        EffectParalyze* pEffectParalyze = NULL;
        EffectSeduction* pEffectSeduction = NULL;

        // Cure the curse effects.
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_DOOM)) {
            pEffectDoom = (EffectDoom*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_DOOM);
            Assert(pEffectDoom != NULL);

            bDoom = HitRoll::isSuccessRemoveCurse(50, SkillLevel, 10, pEffectDoom->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_BLUNTING)) {
            pEffectBlunting = (EffectBlunting*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_BLUNTING);
            Assert(pEffectBlunting != NULL);

            bBlunting = HitRoll::isSuccessRemoveCurse(50, SkillLevel, 15, pEffectBlunting->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_PARALYZE)) {
            pEffectParalyze = (EffectParalyze*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_PARALYZE);
            Assert(pEffectParalyze != NULL);

            bParalyze = HitRoll::isSuccessRemoveCurse(50, SkillLevel, 20, pEffectParalyze->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_SEDUCTION)) {
            pEffectSeduction = (EffectSeduction*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_SEDUCTION);
            Assert(pEffectSeduction != NULL);

            bSeduction = HitRoll::isSuccessRemoveCurse(50, SkillLevel, 30, pEffectSeduction->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }

        // Cure the poison effects.
        bool bGreenPoison = false;    // Whether to cure GreenPoison
        bool bYellowPoison = false;   // Whether to cure YellowPoison
        bool bDarkBluePoison = false; // Whether to cure DarkBluePoison
        bool bGreenStalker = false;   // Whether to cure GreenStalker

        EffectPoison* pEffectPoison = NULL;
        EffectYellowPoisonToCreature* pEffectYellowPoisonToCreature = NULL;
        EffectDarkBluePoison* pEffectDBP = NULL;
        EffectGreenStalker* pEffectGreenStalker = NULL;

        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_POISON)) {
            pEffectPoison = (EffectPoison*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_POISON);
            Assert(pEffectPoison != NULL);

            bGreenPoison = HitRoll::isSuccessCurePoison(50, SkillLevel, 10, pEffectPoison->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }

        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE)) {
            pEffectYellowPoisonToCreature = (EffectYellowPoisonToCreature*)pTargetCreature->findEffect(
                Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
            Assert(pEffectYellowPoisonToCreature != NULL);

            bYellowPoison =
                HitRoll::isSuccessCurePoison(50, SkillLevel, 20, pEffectYellowPoisonToCreature->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }

        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON)) {
            pEffectDBP = (EffectDarkBluePoison*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON);
            Assert(pEffectDBP != NULL);

            bDarkBluePoison = HitRoll::isSuccessCurePoison(50, SkillLevel, 30, pEffectDBP->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }

        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_GREEN_STALKER)) {
            pEffectGreenStalker = (EffectGreenStalker*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_GREEN_STALKER);
            Assert(pEffectGreenStalker != NULL);

            bGreenStalker = HitRoll::isSuccessCurePoison(50, SkillLevel, 40, pEffectGreenStalker->getLevel(), 10);
            bEffected = true;
            bHPCheck = true;
        }

        if (bHPCheck && bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && pTargetCreature->isAlive()) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            uint HealPoint = param.SkillDamage;

            // Clear each curse and
            // add it to the packet as an effect to remove.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pTargetCreature->getObjectID());

            // Clear the blood drain state if the target is under it.
            if (pEffectBloodDrain != NULL && pEffectBloodDrain->getLevel() < CureAllBloodDrainLevel) {
                // Attach an aftermath effect to prevent blood drain farming.
                if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                    Effect* pEffect = pTargetCreature->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                    EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                } else {
                    EffectAftermath* pEffectAftermath = new EffectAftermath(pTargetCreature);
                    pEffectAftermath->setDeadline(5 * 600); // Lasts 5 minutes.
                    pTargetCreature->addEffect(pEffectAftermath);
                    pTargetCreature->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                    pEffectAftermath->create(pTargetCreature->getName());
                }

                pEffectBloodDrain->destroy(pTargetSlayer->getName());
                pTargetSlayer->deleteEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);

                SLAYER_RECORD prev;
                pTargetSlayer->getSlayerRecord(prev);
                pTargetSlayer->initAllStat();
                pTargetSlayer->sendRealWearingInfo();
                pTargetSlayer->addModifyInfo(prev, _GCSkillToObjectOK2);

                gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);
            }

            if (bDoom) {
                pEffectDoom->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_DOOM);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DOOM);
            }
            if (bBlunting) {
                pEffectBlunting->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_BLUNTING);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_BLUNTING);
            }
            if (bParalyze) {
                pEffectParalyze->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_PARALYZE);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_PARALYZE);
            }
            if (bSeduction) {
                pEffectSeduction->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_SEDUCTION);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SEDUCTION);
            }

            // Cure each poison and
            // add it to the packet as an effect to remove.
            if (bGreenPoison) {
                pEffectPoison->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_POISON);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_POISON);
            }
            if (bYellowPoison) {
                pEffectYellowPoisonToCreature->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
            }
            if (bDarkBluePoison) {
                pEffectDBP->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_DARKBLUE_POISON);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DARKBLUE_POISON);
            }
            if (bGreenStalker) {
                pEffectGreenStalker->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_GREEN_STALKER);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GREEN_STALKER);
            }

            if (bEffected) {
                pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(), &gcRemoveEffect);
            }

            // Heal the other creature.
            HP_t CurrentHP = pTargetSlayer->getHP(ATTR_CURRENT);
            HP_t MaxHP = pTargetSlayer->getHP(ATTR_MAX);

            // Compute the actual amount healed.
            int RealHealPoint = 0;
            if (CurrentHP + HealPoint <= MaxHP) {
                RealHealPoint = max((unsigned int)0, HealPoint);
            } else {
                RealHealPoint = max(0, MaxHP - CurrentHP);
            }

            // Raises experience.
            shareAttrExp(pSlayer, RealHealPoint, param.STRMultiplier, param.DEXMultiplier, param.INTMultiplier,
                         _GCSkillToObjectOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);

            // Set the HP.
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

        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void CureAll::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    input.TargetType = SkillInput::TARGET_SELF;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 8;
    param.Level = CureAllBloodDrainLevel;

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(param.SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        Level_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bHPCheck = false;

        EffectBloodDrain* pEffectBloodDrain = NULL;

        bool bEffected = false; // Set when any curable effect is present.
        if (pSlayer->getHP(ATTR_CURRENT) < pSlayer->getHP(ATTR_MAX)) {
            bHPCheck = true;
        }
        if (pSlayer->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
            Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
            pEffectBloodDrain = dynamic_cast<EffectBloodDrain*>(pEffect);
            Assert(pEffectBloodDrain != NULL);

            if (pEffectBloodDrain->getLevel() < param.Level)
                bHPCheck = true;
            bEffected = true;
        }

        bool bDoom = false;
        bool bBlunting = false;
        bool bParalyze = false;
        bool bSeduction = false;

        EffectDoom* pEffectDoom = NULL;
        EffectBlunting* pEffectBlunting = NULL;
        EffectParalyze* pEffectParalyze = NULL;
        EffectSeduction* pEffectSeduction = NULL;

        // Cure the curse effects.
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_DOOM)) {
            pEffectDoom = (EffectDoom*)pSlayer->findEffect(Effect::EFFECT_CLASS_DOOM);
            Assert(pEffectDoom != NULL);

            bDoom = HitRoll::isSuccessRemoveCurse(75, SkillLevel, 10, pEffectDoom->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_BLUNTING)) {
            pEffectBlunting = (EffectBlunting*)pSlayer->findEffect(Effect::EFFECT_CLASS_BLUNTING);
            Assert(pEffectBlunting != NULL);

            bBlunting = HitRoll::isSuccessRemoveCurse(75, SkillLevel, 15, pEffectBlunting->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_PARALYZE)) {
            pEffectParalyze = (EffectParalyze*)pSlayer->findEffect(Effect::EFFECT_CLASS_PARALYZE);
            Assert(pEffectParalyze != NULL);

            bParalyze = HitRoll::isSuccessRemoveCurse(75, SkillLevel, 20, pEffectParalyze->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_SEDUCTION)) {
            pEffectSeduction = (EffectSeduction*)pSlayer->findEffect(Effect::EFFECT_CLASS_SEDUCTION);
            Assert(pEffectSeduction != NULL);

            bSeduction = HitRoll::isSuccessRemoveCurse(75, SkillLevel, 30, pEffectSeduction->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }

        // Cure the poison effects.
        bool bGreenPoison = false;    // Whether to cure GreenPoison
        bool bYellowPoison = false;   // Whether to cure YellowPoison
        bool bDarkBluePoison = false; // Whether to cure DarkBluePoison
        bool bGreenStalker = false;   // Whether to cure GreenStalker

        EffectPoison* pEffectPoison = NULL;
        EffectYellowPoisonToCreature* pEffectYellowPoisonToCreature = NULL;
        EffectDarkBluePoison* pEffectDBP = NULL;
        EffectGreenStalker* pEffectGreenStalker = NULL;

        if (pSlayer->isEffect(Effect::EFFECT_CLASS_POISON)) {
            pEffectPoison = (EffectPoison*)pSlayer->findEffect(Effect::EFFECT_CLASS_POISON);
            Assert(pEffectPoison != NULL);

            bGreenPoison = HitRoll::isSuccessCurePoison(75, SkillLevel, 10, pEffectPoison->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }

        if (pSlayer->isEffect(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE)) {
            pEffectYellowPoisonToCreature =
                (EffectYellowPoisonToCreature*)pSlayer->findEffect(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
            Assert(pEffectYellowPoisonToCreature != NULL);

            bYellowPoison =
                HitRoll::isSuccessCurePoison(75, SkillLevel, 20, pEffectYellowPoisonToCreature->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }

        if (pSlayer->isEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON)) {
            pEffectDBP = (EffectDarkBluePoison*)pSlayer->findEffect(Effect::EFFECT_CLASS_DARKBLUE_POISON);
            Assert(pEffectDBP != NULL);

            bDarkBluePoison = HitRoll::isSuccessCurePoison(75, SkillLevel, 30, pEffectDBP->getLevel(), 25);
            bEffected = true;
            bHPCheck = true;
        }

        if (pSlayer->isEffect(Effect::EFFECT_CLASS_GREEN_STALKER)) {
            pEffectGreenStalker = (EffectGreenStalker*)pSlayer->findEffect(Effect::EFFECT_CLASS_GREEN_STALKER);
            Assert(pEffectGreenStalker != NULL);

            bGreenStalker = HitRoll::isSuccessCurePoison(75, SkillLevel, 40, pEffectGreenStalker->getLevel(), 25);
            bEffected = true;
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

            // Clear each curse and
            // add it to the packet as an effect to remove.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pSlayer->getObjectID());

            if (bDoom) {
                pEffectDoom->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_DOOM);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DOOM);
            }
            if (bBlunting) {
                pEffectBlunting->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_BLUNTING);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_BLUNTING);
            }
            if (bParalyze) {
                pEffectParalyze->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_PARALYZE);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_PARALYZE);
            }
            if (bSeduction) {
                pEffectSeduction->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_SEDUCTION);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SEDUCTION);
            }

            // Cure each poison and
            // add it to the packet as an effect to remove.
            if (bGreenPoison) {
                pEffectPoison->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_POISON);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_POISON);
            }
            if (bYellowPoison) {
                pEffectYellowPoisonToCreature->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_YELLOW_POISON_TO_CREATURE);
            }
            if (bDarkBluePoison) {
                pEffectDBP->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_DARKBLUE_POISON);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DARKBLUE_POISON);
            }
            if (bGreenStalker) {
                pEffectGreenStalker->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_GREEN_STALKER);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_GREEN_STALKER);
            }

            if (bEffected) {
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

        } else {
            executeSkillFailNormal(pSlayer, param.SkillType, NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, param.SkillType);
    }
}

CureAll g_CureAll;
