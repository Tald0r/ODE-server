//////////////////////////////////////////////////////////////////////////////
// Filename    : WildWolf.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "WildWolf.h"

#include "Corpse.h"
#include "CrossCounter.h"
#include "EffectHPRecovery.h"
#include "EffectSetAfire.h"
#include "GCDeleteObject.h"
#include "GCHPRecoveryStartToOthers.h"
#include "GCHPRecoveryStartToSelf.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK5.h"
#include "GameContext.h"
#include "HitRoll.h"
#include "MonsterCorpse.h"
#include "RankBonus.h"
#include "SkillUtil.h"
#include "Vampire.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void WildWolf::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
                       CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);
        if (pTargetCreature == NULL) {
            Item* pTargetItem = pZone->getItem(TargetObjectID);
            if (pTargetItem == NULL || pTargetItem->getItemClass() != Item::ITEM_CLASS_CORPSE) {
                executeSkillFailException(pVampire, getSkillType());
                return;
            }

            eatCorpse(pVampire, pTargetItem, pVampireSkillSlot);
            return;
        }

        // An NPC cannot be attacked.
        // A missing target fails the skill instead of throwing.
        if (!canAttack(pVampire, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
            eatComaCreature(pVampire, pTargetCreature);
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        Range_t Range = pSkillInfo->getRange();

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, pTargetCreature, Range);
        bool bHitRoll = HitRoll::isSuccess(pVampire, pTargetCreature);
        bool bCanHit = canHit(pVampire, pTargetCreature, getSkillType());
        bool bPK = verifyPK(pVampire, pTargetCreature);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK) {
            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            Damage_t Damage = output.Damage;
            bool bCriticalHit = false;
            Damage += computeDamage(pVampire, pTargetCreature, 0, bCriticalHit);

            // Consume the mana.
            decreaseMana(pVampire, RequiredMP, _GCSkillToObjectOK1);

            EffectSetAfire* pEffect = new EffectSetAfire(pTargetCreature);
            pEffect->setDamage(Damage);
            pEffect->setTick(3);
            pEffect->setNextTime(3);
            pEffect->setTimes(3);
            pEffect->setCasterOID(pVampire->getObjectID());
            pTargetCreature->addEffect(pEffect);

            increaseAlignment(pVampire, pTargetCreature, _GCSkillToObjectOK1);

            // Send the packet.
            _GCSkillToObjectOK1.setSkillType(getSkillType());
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK2.setSkillType(getSkillType());
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK5.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setSkillType(getSkillType());

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            Player* pTargetPlayer = NULL;

            if (pTargetCreature->isPC()) {
                pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pVampire);
            }

            list<Creature*> cList;
            cList.push_back(pVampire);
            cList.push_back(pTargetCreature);

            cList = pZone->broadcastSkillPacket(pVampire->getX(), pVampire->getY(), pTargetCreature->getX(),
                                                pTargetCreature->getY(), &_GCSkillToObjectOK5, cList);

            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

void WildWolf::eatCorpse(Vampire* pVampire, Item* pCorpse, VampireSkillSlot* pVampireSkillSlot) {
    if (pCorpse->getItemClass() != Item::ITEM_CLASS_CORPSE || pCorpse->getItemType() == VAMPIRE_CORPSE) {
        executeSkillFailException(pVampire, getSkillType());
        return;
    }

    if (pCorpse != NULL && pCorpse->getItemClass() == Item::ITEM_CLASS_CORPSE &&
        pCorpse->getItemType() == MONSTER_CORPSE) {
        MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pCorpse);
        Assert(pMonsterCorpse != NULL);

        if (pMonsterCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE) ||
            pMonsterCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE) ||
            pMonsterCorpse->getMonsterType() == 482 || pMonsterCorpse->getMonsterType() == 650 ||
            pMonsterCorpse->isShrine()) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }
    }

    Corpse* pTargetCorpse = dynamic_cast<Corpse*>(pCorpse);
    Assert(pTargetCorpse != NULL);

    Zone* pZone = pVampire->getZone();

    GCSkillToObjectOK1 _GCSkillToObjectOK1;
    GCSkillToObjectOK5 _GCSkillToObjectOK5;

    SkillType_t SkillType = pVampireSkillSlot->getSkillType();
    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
    Range_t Range = pSkillInfo->getRange();

    int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
    bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
    bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
    bool bRangeCheck =
        getDistance(pVampire->getX(), pVampire->getY(), pTargetCorpse->getX(), pTargetCorpse->getY()) <= Range;

    if (bManaCheck && bTimeCheck && bRangeCheck) {
        SkillInput input(pVampire);
        SkillOutput output;
        computeOutput(input, output);

        int RegenPeriod = 100;
        int Period = 15;
        int RegenHPUnit = 30;
        int Deadline = RegenPeriod * Period;

        if (pVampire->isFlag(Effect::EFFECT_CLASS_HP_RECOVERY)) {
            Effect* pEffect = pVampire->findEffect(Effect::EFFECT_CLASS_HP_RECOVERY);
            EffectHPRecovery* pEffectHPRecoveryEffect = dynamic_cast<EffectHPRecovery*>(pEffect);

            // Update how many more times the recovery has to run.
            Turn_t OldCount = pEffectHPRecoveryEffect->getPeriod();
            Turn_t NewPeriod = OldCount + Period;
            pEffectHPRecoveryEffect->setPeriod(NewPeriod);
            pEffectHPRecoveryEffect->setDeadline(NewPeriod * RegenPeriod);

            // Send the recovery start packet to the caster.
            GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
            gcHPRecoveryStartToSelf.setPeriod(NewPeriod);     // Number of recovery ticks
            gcHPRecoveryStartToSelf.setDelay(RegenPeriod);    // Seconds between ticks
            gcHPRecoveryStartToSelf.setQuantity(RegenHPUnit); // Amount recovered each tick

            pVampire->getPlayer()->sendPacket(&gcHPRecoveryStartToSelf);

            // Send the recovery start packet to the others.
            // The refresh packet is the same packet as the start packet.
            GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
            gcHPRecoveryStartToOthers.setObjectID(pVampire->getObjectID());
            gcHPRecoveryStartToOthers.setPeriod(NewPeriod);
            gcHPRecoveryStartToOthers.setDelay(RegenPeriod);
            gcHPRecoveryStartToOthers.setQuantity(RegenHPUnit);

            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcHPRecoveryStartToOthers, pVampire);
        } else {
            EffectHPRecovery* pEffectHPRecovery = new EffectHPRecovery();

            pEffectHPRecovery->setTarget(pVampire);
            pEffectHPRecovery->setDeadline(Deadline);
            pEffectHPRecovery->setDelay(RegenPeriod);
            pEffectHPRecovery->setNextTime(0);
            pEffectHPRecovery->setHPQuantity(RegenHPUnit);
            pEffectHPRecovery->setPeriod(Period);

            pVampire->addEffect(pEffectHPRecovery);

            // Send the recovery start packet to the caster.
            GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
            gcHPRecoveryStartToSelf.setPeriod(Period);
            gcHPRecoveryStartToSelf.setDelay(RegenPeriod);
            gcHPRecoveryStartToSelf.setQuantity(RegenHPUnit);

            pVampire->getPlayer()->sendPacket(&gcHPRecoveryStartToSelf);

            // Send the recovery start packet to the watchers.
            GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
            gcHPRecoveryStartToOthers.setObjectID(pVampire->getObjectID());
            gcHPRecoveryStartToOthers.setPeriod(Period);
            gcHPRecoveryStartToOthers.setDelay(RegenPeriod);
            gcHPRecoveryStartToOthers.setQuantity(RegenHPUnit);

            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcHPRecoveryStartToOthers, pVampire);
        }

        // Send the packet.
        _GCSkillToObjectOK1.setSkillType(getSkillType());
        _GCSkillToObjectOK1.setCEffectID(0);
        _GCSkillToObjectOK1.setTargetObjectID(pTargetCorpse->getObjectID());
        _GCSkillToObjectOK1.setDuration(0);
        _GCSkillToObjectOK1.setGrade(1);

        _GCSkillToObjectOK5.setObjectID(pVampire->getObjectID());
        _GCSkillToObjectOK5.setTargetObjectID(pTargetCorpse->getObjectID());
        _GCSkillToObjectOK5.setSkillType(getSkillType());
        _GCSkillToObjectOK5.setGrade(1);

        pVampire->getPlayer()->sendPacket(&_GCSkillToObjectOK1);

        list<Creature*> cList;
        cList.push_back(pVampire);

        cList = pZone->broadcastSkillPacket(pVampire->getX(), pVampire->getY(), pTargetCorpse->getX(),
                                            pTargetCorpse->getY(), &_GCSkillToObjectOK5, cList);

        pVampireSkillSlot->setRunTime(output.Delay);

        // Broadcast the packet that makes the corpse disappear.
        GCDeleteObject gcDO;
        gcDO.setObjectID(pTargetCorpse->getObjectID());
        pZone->broadcastPacket(pTargetCorpse->getX(), pTargetCorpse->getY(), &gcDO);
        // Delete it from the zone.
        pZone->deleteItem(pTargetCorpse, pTargetCorpse->getX(), pTargetCorpse->getY());
        // The zone does not actually free the pointer, so delete it here.
        SAFE_DELETE(pTargetCorpse);
    }
}

void WildWolf::eatComaCreature(Vampire* pVampire, Creature* pComaCreature) {
    executeSkillFailNormal(pVampire, getSkillType(), pComaCreature);
}

WildWolf g_WildWolf;
