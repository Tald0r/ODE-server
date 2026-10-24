//////////////////////////////////////////////////////////////////////////////
// Filename    : Resurrect.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Resurrect.h"

#include "EffectComa.h"
#include "EffectKillAftermath.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "KernelContext.h"
#include "Properties.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void Resurrect::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // Only a Slayer can be resurrected.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !pTargetCreature->isSlayer() ||
            (de::kernelContext().config().hasKey("Hardcore") &&
             de::kernelContext().config().getPropertyInt("Hardcore") != 0)) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetCreature);
        Assert(pTargetSlayer != NULL);

        // Cannot be used unless the target is dead and under the coma effect.
        if (!pTargetSlayer->isFlag(Effect::EFFECT_CLASS_COMA) || !pTargetSlayer->isDead()) {
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

        ZoneCoord_t myX = pSlayer->getX();
        ZoneCoord_t myY = pSlayer->getY();
        ZoneCoord_t targetX = pTargetCreature->getX();
        ZoneCoord_t targetY = pTargetCreature->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bCanResurrect = false;

        EffectComa* pEffectComa = (EffectComa*)(pTargetCreature->findEffect(Effect::EFFECT_CLASS_COMA));
        Assert(pEffectComa != NULL);

        if (pEffectComa->canResurrect()) {
            bCanResurrect = true;
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanResurrect) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Delete the coma effect from the target's effect manager.
            pTargetCreature->deleteEffect(Effect::EFFECT_CLASS_COMA);
            pTargetCreature->removeFlag(Effect::EFFECT_CLASS_COMA);

            // Announce that the coma effect is gone.
            GCRemoveEffect gcRemoveEffect;
            gcRemoveEffect.setObjectID(pTargetSlayer->getObjectID());
            gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_COMA);
            pZone->broadcastPacket(targetX, targetY, &gcRemoveEffect);

            // Send the effect info again.
            pTargetSlayer->getEffectManager()->sendEffectInfo(pTargetSlayer, pZone, pTargetSlayer->getX(),
                                                              pTargetSlayer->getY());

            // Attach the Aftermath effect to prevent resurrect farming.
            if (pTargetSlayer->isFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH)) {
                Effect* pEffect = pTargetSlayer->findEffect(Effect::EFFECT_CLASS_KILL_AFTERMATH);
                EffectKillAftermath* pEffectKillAftermath = dynamic_cast<EffectKillAftermath*>(pEffect);
                pEffectKillAftermath->setDeadline(5 * 600);
            } else {
                EffectKillAftermath* pEffectKillAftermath = new EffectKillAftermath(pTargetSlayer);
                pEffectKillAftermath->setDeadline(5 * 600);
                pTargetSlayer->addEffect(pEffectKillAftermath);
                pTargetSlayer->setFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH);
                pEffectKillAftermath->create(pTargetSlayer->getName());
            }

            // Restore only 10% of the target's HP.
            HP_t CurrentHP = getPercentValue(pTargetSlayer->getHP(ATTR_MAX), 10);
            pTargetSlayer->setHP(CurrentHP, ATTR_CURRENT);
            pTargetSlayer->setMP(0, ATTR_CURRENT);

            // Tell nearby players that the HP was restored.
            GCStatusCurrentHP gcStatusCurrentHP;
            gcStatusCurrentHP.setObjectID(pTargetSlayer->getObjectID());
            gcStatusCurrentHP.setCurrentHP(pTargetSlayer->getHP(ATTR_CURRENT));
            pZone->broadcastPacket(targetX, targetY, &gcStatusCurrentHP);

            // Raises experience.
            shareAttrExp(pSlayer, pSkillInfo->getPoint(), 1, 1, 8, _GCSkillToObjectOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);
            increaseAlignment(pSlayer, pTargetCreature, _GCSkillToObjectOK1);

            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(0);
            _GCSkillToObjectOK2.addShortData(MODIFY_CURRENT_MP, pTargetSlayer->getMP(ATTR_CURRENT));

            _GCSkillToObjectOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            Player* pTargetPlayer = pTargetSlayer->getPlayer();
            Assert(pTargetPlayer != NULL);
            pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);

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

Resurrect g_Resurrect;
