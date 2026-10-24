//////////////////////////////////////////////////////////////////////////////
// Filename    : Sniping.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Sniping.h"

#include "EffectFadeOut.h"
#include "EffectSnipingMode.h"
#include "GCDeleteObject.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "ItemUtil.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Sniping::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // Cannot be used without a gun in the right hand, or while a relic is held.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || isArmsWeapon(pItem) == false || pSlayer->hasRelicItem()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t x = pSlayer->getX();
        ZoneCoord_t y = pSlayer->getY();

        Tile& rTile = pZone->getTile(pSlayer->getX(), pSlayer->getY());
        int RequiredMP = pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected =
            pSlayer->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) || pSlayer->isFlag(Effect::EFFECT_CLASS_FADE_OUT) ||
            pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER) ||
            rTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            // Stats are recalculated when EffectFadeOut is unaffected,
            // that is, when EffectSnipingMode is attached.
            EffectFadeOut* pEffect = new EffectFadeOut(pSlayer);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(40);
            pEffect->setSniping();
            pSlayer->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_FADE_OUT);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 8, 1, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            // Send the packet.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

void Sniping::checkRevealRatio(Creature* pCreature, int base, int divisor) {
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());
    Assert(pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE));

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Assert(pSlayer != NULL);

    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    SkillSlot* pSkillSlot = pSlayer->getSkill(SKILL_SNIPING);
    Assert(pSkillSlot != NULL);

    EffectSnipingMode* pEffectSM =
        dynamic_cast<EffectSnipingMode*>(pCreature->findEffect(Effect::EFFECT_CLASS_SNIPING_MODE));
    Assert(pEffectSM != NULL);

    int penalty = base - (pSkillSlot->getExpLevel() / divisor);
    pEffectSM->setRevealRatio(pEffectSM->getRevealRatio() + penalty);

    if (rand() % 100 < pEffectSM->getRevealRatio()) {
        // Turn the effect off.
        //  2003. 1. 17 by bezz
        pEffectSM->setDeadline(0);
    }
}

Sniping g_Sniping;
