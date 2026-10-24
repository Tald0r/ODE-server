//////////////////////////////////////////////////////////////////////////////
// Filename    : HolyArmor.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "HolyArmor.h"

#include "EffectHolyArmor.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self
//////////////////////////////////////////////////////////////////////////////
void HolyArmor::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // The skill cannot be used if no weapon is equipped or it is not a sword.

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP) && pSlayer->getHP() > 100;
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_HOLY_ARMOR);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            pSlayer->setHP(pSlayer->getHP() - 100);
            _GCSkillToSelfOK1.addShortData(MODIFY_CURRENT_HP, pSlayer->getHP());

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectHolyArmor* pEffect = new EffectHolyArmor(pSlayer);
            pEffect->setDeadline(output.Duration);
            pEffect->setDefBonus(output.Damage);
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_HOLY_ARMOR);

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

            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pSlayer->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_HOLY_ARMOR);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(X, Y, &gcAddEffect);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);

            uint PartyID = pSlayer->getPartyID();
            if (PartyID != 0) {
                LocalPartyManager* pLPM = pZone->getLocalPartyManager();
                pLPM->shareHolyArmor(PartyID, pSlayer, output.Damage, input.SkillLevel);
            }
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

HolyArmor g_HolyArmor;
