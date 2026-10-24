//////////////////////////////////////////////////////////////////////////////
// Filename    : PotentialExplosion.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PotentialExplosion.h"

#include "EffectPotentialExplosion.h"
#include "GCAddEffect.h"
#include "GCOtherModifyInfo.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "PacketUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void PotentialExplosion::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    SkillType_t SkillType = pSkillSlot->getSkillType();
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

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // The bonus is a little larger when HP is below half.
            // by sigi. 2002.12.3
            if (pSlayer->getHP(ATTR_CURRENT) < (pSlayer->getHP(ATTR_MAX) / 2)) {
                output.Damage = 8 + input.SkillLevel / 15;
            } else {
                output.Damage = 3 + input.SkillLevel / 20;
            }

            int diffSTR = output.Damage;
            int diffDEX = output.Damage;

            EffectPotentialExplosion* pEffect = new EffectPotentialExplosion(pSlayer);
            pEffect->setDeadline(output.Duration);
            pEffect->setDiffSTR(diffSTR);
            pEffect->setDiffDEX(diffDEX);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION);

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
            shareAttrExp(pSlayer, ExpUp, 8, 1, 1, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);

            // Sends the packet.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_POTENTIAL_EXPLOSION);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddEffect);

            GCOtherModifyInfo gcOtherModifyInfo;
            makeGCOtherModifyInfo(&gcOtherModifyInfo, pSlayer, &prev);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcOtherModifyInfo, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

PotentialExplosion g_PotentialExplosion;
