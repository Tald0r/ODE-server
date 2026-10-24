//////////////////////////////////////////////////////////////////////////////
// Filename    : Berserker.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Berserker.h"

#include "EffectBerserker.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Berserker::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // Cannot be used if no weapon is equipped or it is not a blade.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_BLADE) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        bool bIncreaseDomainExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        // Cannot be used at the same time as Charging Power.
        bool bEffected =
            pSlayer->isFlag(Effect::EFFECT_CLASS_BERSERKER) || pSlayer->isFlag(Effect::EFFECT_CLASS_CHARGING_POWER);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            int DefensePenalty = 25 - pSkillSlot->getExpLevel() * 2 / 13;    // %
            int ProtectionPenalty = 20 - pSkillSlot->getExpLevel() * 2 / 13; // %

            // Create the effect class and attach it.
            EffectBerserker* pEffect = new EffectBerserker(pSlayer);
            pEffect->setDeadline(output.Duration);
            pEffect->setDefensePenalty(DefensePenalty);
            pEffect->setProtectionPenalty(ProtectionPenalty);
            pEffect->setToHitBonus(output.ToHit);
            pEffect->setDamageBonus(output.Damage);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_BERSERKER);

            // Send the stats that this changes.
            SLAYER_RECORD prev;
            pSlayer->getSlayerRecord(prev);
            pSlayer->initAllStat();
            pSlayer->sendRealWearingInfo();
            pSlayer->sendModifyInfo(prev);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);

            if (bIncreaseDomainExp) {
                shareAttrExp(pSlayer, ExpUp, 8, 1, 1, _GCSkillToSelfOK1);
                increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            }

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_BERSERKER);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

Berserker g_Berserker;
