//////////////////////////////////////////////////////////////////////////////
// Filename    : RemoveCurse.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "RemoveCurse.h"

#include "EffectDeath.h"
#include "EffectDoom.h"
#include "EffectHallucination.h"
#include "EffectParalyze.h"
#include "EffectSeduction.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void RemoveCurse::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // Only Slayers can be cured.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || pTargetCreature->isSlayer() == false) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;
        GCSkillToObjectOK4 _GCSkillToObjectOK4;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        // by sigi. 2002.12.3
        SkillLevel_t SkillLevel = pSlayer->getINT() / 2; // pSkillSlot->getExpLevel();

        bool bDoom = false;
        bool bParalyze = false;
        bool bSeduction = false;
        bool bDeath = false;
        bool bEffected = false; // Set when any curse is present.

        EffectDoom* pEffectDoom = NULL;
        EffectParalyze* pEffectParalyze = NULL;
        EffectSeduction* pEffectSeduction = NULL;
        EffectDeath* pEffectDeath = NULL;

        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_DOOM)) {
            pEffectDoom = (EffectDoom*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_DOOM);
            Assert(pEffectDoom != NULL);

            bDoom = HitRoll::isSuccessRemoveCurse(45, SkillLevel, 20, pEffectDoom->getLevel() / 4);
            bEffected = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_PARALYZE)) {
            pEffectParalyze = (EffectParalyze*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_PARALYZE);
            Assert(pEffectParalyze != NULL);

            bParalyze = HitRoll::isSuccessRemoveCurse(45, SkillLevel, 10, pEffectParalyze->getLevel() / 4);
            bEffected = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_SEDUCTION)) {
            pEffectSeduction = (EffectSeduction*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_SEDUCTION);
            Assert(pEffectSeduction != NULL);

            bSeduction = HitRoll::isSuccessRemoveCurse(45, SkillLevel, 30, pEffectSeduction->getLevel() / 4);
            bEffected = true;
        }
        if (pTargetCreature->isEffect(Effect::EFFECT_CLASS_DEATH)) {
            pEffectDeath = (EffectDeath*)pTargetCreature->findEffect(Effect::EFFECT_CLASS_DEATH);
            Assert(pEffectDeath != NULL);

            bDeath = HitRoll::isSuccessRemoveCurse(45, SkillLevel, 50, pEffectDeath->getLevel() / 4);
            bEffected = true;
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());

        // There must be mana, the delay must have passed, the distance must be right,
        // At least one curse must be active.
        if (bManaCheck && bTimeCheck && bRangeCheck && bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Clear each curse and
            // add it to the packet as an effect to remove.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pTargetCreature->getObjectID());

            if (bDoom) {
                pEffectDoom->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_DOOM);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DOOM);
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
            if (bDeath) {
                pEffectDeath->setDeadline(0);
                pTargetCreature->removeFlag(Effect::EFFECT_CLASS_DEATH);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DEATH);
            }

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToObjectOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);

            // Build the packet and send it.
            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();
            ZoneCoord_t targetX = pTargetCreature->getX();
            ZoneCoord_t targetY = pTargetCreature->getY();

            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            }

            pZone->broadcastPacket(targetX, targetY, &gcRemoveEffect);

            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList.push_back(pTargetCreature);

            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCSkillToObjectOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void RemoveCurse::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        // by sigi. 2002.12.3
        SkillLevel_t SkillLevel = pSlayer->getINT() / 2; // pSkillSlot->getExpLevel();

        bool bDoom = false;
        bool bParalyze = false;
        bool bSeduction = false;
        bool bDeath = false;
        bool bEffected = false; // Set when any of the curses is active.

        EffectDoom* pEffectDoom = NULL;
        EffectParalyze* pEffectParalyze = NULL;
        EffectSeduction* pEffectSeduction = NULL;
        EffectDeath* pEffectDeath = NULL;

        if (pSlayer->isEffect(Effect::EFFECT_CLASS_DOOM)) {
            pEffectDoom = (EffectDoom*)pSlayer->findEffect(Effect::EFFECT_CLASS_DOOM);
            Assert(pEffectDoom != NULL);

            bDoom = HitRoll::isSuccessRemoveCurse(70, SkillLevel, 20, pEffectDoom->getLevel() / 4);
            bEffected = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_PARALYZE)) {
            pEffectParalyze = (EffectParalyze*)pSlayer->findEffect(Effect::EFFECT_CLASS_PARALYZE);
            Assert(pEffectParalyze != NULL);

            bParalyze = HitRoll::isSuccessRemoveCurse(70, SkillLevel, 10, pEffectParalyze->getLevel() / 4);
            bEffected = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_SEDUCTION)) {
            pEffectSeduction = (EffectSeduction*)pSlayer->findEffect(Effect::EFFECT_CLASS_SEDUCTION);
            Assert(pEffectSeduction != NULL);

            bSeduction = HitRoll::isSuccessRemoveCurse(70, SkillLevel, 30, pEffectSeduction->getLevel() / 4);
            bEffected = true;
        }
        if (pSlayer->isEffect(Effect::EFFECT_CLASS_DEATH)) {
            pEffectDeath = (EffectDeath*)pSlayer->findEffect(Effect::EFFECT_CLASS_DEATH);
            Assert(pEffectDeath != NULL);

            bDeath = HitRoll::isSuccessRemoveCurse(70, SkillLevel, 50, pEffectDeath->getLevel() / 4);
            bEffected = true;
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);

        // There must be mana, the delay must have passed, the distance must be right,
        // At least one curse must be active.
        if (bManaCheck && bTimeCheck && bRangeCheck && bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Remove each of the curses,
            // add it to the packet as an effect to remove.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pSlayer->getObjectID());

            if (bDoom) {
                pEffectDoom->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_DOOM);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DOOM);
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
            if (bDeath) {
                pEffectDeath->setDeadline(0);
                pSlayer->removeFlag(Effect::EFFECT_CLASS_DEATH);
                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DEATH);
            }

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(myX, myY, &_GCSkillToSelfOK2, pSlayer);

            // Notifies that the skill has been removed.
            pZone->broadcastPacket(myX, myY, &gcRemoveEffect);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

RemoveCurse g_RemoveCurse;
