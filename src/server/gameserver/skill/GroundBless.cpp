//////////////////////////////////////////////////////////////////////////////
// Filename    : GroundBless.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GroundBless.h"

#include "EffectGroundBless.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToObjectOK6.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void GroundBless::execute(Ousters* pOusters, OustersSkillSlot* pOustersSkillSlot, CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pOustersSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pOustersSkillSlot->getSkillType());
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pOustersSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP() + pOustersSkillSlot->getExpLevel() / 2;
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pOustersSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pOusters);
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pOustersSkillSlot);
        bool bEffected = pOusters->isFlag(Effect::EFFECT_CLASS_GROUND_BLESS);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pOusters, pOustersSkillSlot);
            SkillOutput output;

            input.STR = pOusters->getSTR(ATTR_BASIC);
            input.DEX = pOusters->getDEX(ATTR_BASIC);
            input.INTE = pOusters->getINT(ATTR_BASIC);

            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectGroundBless* pEffect = new EffectGroundBless(pOusters);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pOusters->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_GROUND_BLESS);

            pOusters->initAllStatAndSend();

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCSkillToSelfOK2, pOusters);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pOusters->getObjectID());
            gcAddEffect.setEffectID(pEffect->getEffectClass());
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect);

            pOustersSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

void GroundBless::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pSkillSlot,
                          CEffectID_t CEffectID) {
    Assert(pOusters != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Item* pWeapon = pOusters->getWearItem(Ousters::WEAR_RIGHTHAND);
        if (pWeapon == NULL || pWeapon->getItemClass() != Item::ITEM_CLASS_OUSTERS_WRISTLET ||
            !pOusters->isRealWearingEx(Ousters::WEAR_RIGHTHAND)) {
            executeSkillFailException(pOusters, pSkillSlot->getSkillType());
            return;
        }

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // NPCs cannot be attacked.
        // It also fails if the target is immune to curses.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !pTargetCreature->isOusters()) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;
        GCSkillToObjectOK4 _GCSkillToObjectOK4;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;
        GCSkillToObjectOK6 _GCSkillToObjectOK6;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        SkillInput input(pOusters, pSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        int RequiredMP = (int)pSkillInfo->getConsumeMP() + pSkillSlot->getExpLevel() / 2;
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pOusters, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pOusters, pSkillInfo, pSkillSlot);
        bool bEffected = pTargetCreature->isFlag(Effect::EFFECT_CLASS_GROUND_BLESS);
        bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);

        ZoneCoord_t targetX = pTargetCreature->getX();
        ZoneCoord_t targetY = pTargetCreature->getY();
        ZoneCoord_t myX = pOusters->getX();
        ZoneCoord_t myY = pOusters->getY();

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && bSatisfyRequire) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToObjectOK1);

            bool bCanSeeCaster = canSee(pTargetCreature, pOusters);

            // Create the effect object and attach it.
            EffectGroundBless* pEffect = new EffectGroundBless(pTargetCreature);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pTargetCreature->addEffect(pEffect);
            pTargetCreature->setFlag(Effect::EFFECT_CLASS_GROUND_BLESS);

            Ousters* pTargetOusters = dynamic_cast<Ousters*>(pTargetCreature);
            if (pTargetOusters != NULL)
                pTargetOusters->initAllStatAndSend();

            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(output.Duration);

            _GCSkillToObjectOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(output.Duration);

            _GCSkillToObjectOK3.setObjectID(pOusters->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK4.setDuration(output.Duration);

            _GCSkillToObjectOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(output.Duration);

            _GCSkillToObjectOK6.setXY(myX, myY);
            _GCSkillToObjectOK6.setSkillType(SkillType);
            _GCSkillToObjectOK6.setDuration(output.Duration);

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pOusters);
            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCSkillToObjectOK5, cList);

            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            Player* pTargetPlayer = pTargetCreature->getPlayer();

            if (pTargetPlayer != NULL) {
                if (bCanSeeCaster)
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
                else
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK6);
            }

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(TargetObjectID);
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_GROUND_BLESS);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(targetX, targetY, &gcAddEffect);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

GroundBless g_GroundBless;
