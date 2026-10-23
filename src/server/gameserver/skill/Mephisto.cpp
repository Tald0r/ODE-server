//////////////////////////////////////////////////////////////////////////////
// Filename    : Mephisto.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////
#include "Mephisto.h"

#include "EffectMephisto.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GameContext.h"
#include "HitRoll.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Mephisto::execute(Vampire* pVampire, VampireSkillSlot* pVampireSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        // Hit bonus when Knowledge of Curse is present
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_CURSE)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_CURSE);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot, HitBonus);
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_MEPHISTO);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToSelfOK1);

            // The damage bonus varies with the skill level.
            SkillInput input(pVampire);
            SkillOutput output;
            input.SkillLevel = pVampire->getSTR() + pVampire->getDEX() + pVampire->getINT();
            input.DomainLevel = pVampire->getLevel();
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectMephisto* pEffect = new EffectMephisto(pVampire);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pVampire->addEffect(pEffect);
            pVampire->setFlag(Effect::EFFECT_CLASS_MEPHISTO);

            // Send the stats that this changes.
            VAMPIRE_RECORD prev;
            pVampire->getVampireRecord(prev);
            pVampire->initAllStat();
            pVampire->sendRealWearingInfo();
            pVampire->sendModifyInfo(prev);

            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &_GCSkillToSelfOK2, pVampire);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pVampire->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_MEPHISTO);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcAddEffect);

            // set Next Run Time
            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

void Mephisto::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pVampireSkillSlot);
        bool bEffected = pTargetCreature->isFlag(Effect::EFFECT_CLASS_MEPHISTO);

        ZoneCoord_t myX = pVampire->getX();
        ZoneCoord_t myY = pVampire->getY();

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected && pTargetCreature->isVampire()) {
            Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetCreature);

            ZoneCoord_t X = pTargetVampire->getX();
            ZoneCoord_t Y = pTargetVampire->getY();

            decreaseMana(pVampire, RequiredMP, _GCSkillToObjectOK1);

            // The damage bonus varies with the skill level.
            SkillInput input(pVampire);
            SkillOutput output;
            input.SkillLevel = pVampire->getSTR() + pVampire->getDEX() + pVampire->getINT();
            input.DomainLevel = pVampire->getLevel();
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectMephisto* pEffect = new EffectMephisto(pTargetVampire);
            pEffect->setDeadline(output.Duration);
            pEffect->setBonus(output.Damage);
            pTargetVampire->addEffect(pEffect);
            pTargetVampire->setFlag(Effect::EFFECT_CLASS_MEPHISTO);

            // Send the stats that this changes.
            VAMPIRE_RECORD prev;
            pTargetVampire->getVampireRecord(prev);
            pTargetVampire->initAllStat();
            pTargetVampire->sendRealWearingInfo();
            pTargetVampire->sendModifyInfo(prev);

            // Build the packet and send it.
            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setDuration(output.Duration);

            _GCSkillToObjectOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(output.Duration);

            _GCSkillToObjectOK3.setObjectID(pVampire->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(X, Y);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);

                _GCSkillToObjectOK2.setObjectID(pVampire->getObjectID());

                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else {
                Assert(false);
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pVampire);
            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);

            // Notifies that the effect has been attached.
            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pTargetVampire->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_MEPHISTO);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pTargetVampire->getX(), pTargetVampire->getY(), &gcAddEffect);

            // set Next Run Time
            pVampireSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

void Mephisto::execute(Vampire* pVampire)

{
    Assert(pVampire != NULL);

    if (!pVampire->hasSkill(SKILL_MEPHISTO))
        return;

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);


        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK3 _GCSkillToObjectOK3;

        ZoneCoord_t myX = pVampire->getX();
        ZoneCoord_t myY = pVampire->getY();

        int oX, oY;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (oX = -4; oX <= 4; oX++)
            for (oY = -4; oY <= 4; oY++) {
                int tileX = myX + oX;
                int tileY = myY + oY;
                if (!rect.ptInRect(tileX, tileY))
                    continue;

                // Checks whether a Vampire is on the tile.
                Tile& tile = pZone->getTile(tileX, tileY);
                Creature* pTargetCreature = NULL;
                if (tile.hasCreature(Creature::MOVE_MODE_WALKING))
                    pTargetCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);

                if (pTargetCreature != NULL && pTargetCreature != pVampire && pTargetCreature->isVampire()) {
                    bool bEffected = pTargetCreature->isFlag(Effect::EFFECT_CLASS_MEPHISTO);

                    if (bEffected)
                        continue;

                    Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetCreature);

                    // The damage bonus varies with the skill level.
                    SkillInput input(pVampire);
                    SkillOutput output;
                    input.SkillLevel = pVampire->getSTR() + pVampire->getDEX() + pVampire->getINT();
                    input.DomainLevel = pVampire->getLevel();
                    computeOutput(input, output);

                    // Creates the effect class and attaches it.
                    EffectMephisto* pEffect = new EffectMephisto(pTargetVampire);
                    pEffect->setDeadline(output.Duration);
                    pEffect->setBonus(output.Damage);
                    pTargetVampire->addEffect(pEffect);
                    pTargetVampire->setFlag(Effect::EFFECT_CLASS_MEPHISTO);

                    // Sends the stats that change as a result.
                    VAMPIRE_RECORD prev;
                    pTargetVampire->getVampireRecord(prev);
                    pTargetVampire->initAllStat();
                    pTargetVampire->sendRealWearingInfo();
                    pTargetVampire->sendModifyInfo(prev);

                    if (pTargetCreature->isPC()) {
                        Player* pTargetPlayer = pTargetCreature->getPlayer();
                        Assert(pTargetPlayer != NULL);

                        pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
                    } else {
                        Assert(false);
                    }

                    // Notifies that the effect has been attached.
                    GCAddEffect gcAddEffect;
                    gcAddEffect.setObjectID(pTargetVampire->getObjectID());
                    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_MEPHISTO);
                    gcAddEffect.setDuration(output.Duration);
                    pZone->broadcastPacket(pTargetVampire->getX(), pTargetVampire->getY(), &gcAddEffect);
                }
            }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}


Mephisto g_Mephisto;
