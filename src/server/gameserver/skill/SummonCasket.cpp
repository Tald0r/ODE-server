//////////////////////////////////////////////////////////////////////////////
// Filename    : SummonCasket.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SummonCasket.h"

#include "EffectDoom.h"
#include "EffectFlare.h"
#include "EffectSeduction.h"
#include "EffectSummonCasket.h"
#include "GCAddEffect.h"
#include "GCDeleteObject.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK3.h"
#include "GameContext.h"
#include "RankBonus.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire self handler
//////////////////////////////////////////////////////////////////////////////
void SummonCasket::execute(Vampire* pVampire, VampireSkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pSkillSlot != NULL);


    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        if (!pVampire->isVampire() && !pVampire->isMonster()) {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
            return;
        }


        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK3 _GCSkillToSelfOK3;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pVampire->getX();
        ZoneCoord_t y = pVampire->getY();

        // Knowledge of Summon gives a hit bonus of 10.
        int HitBonus = 0;
        if (pVampire->hasRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_SUMMON)) {
            RankBonus* pRankBonus = pVampire->getRankBonus(RankBonus::RANK_BONUS_KNOWLEDGE_OF_SUMMON);
            Assert(pRankBonus != NULL);

            HitBonus = pRankBonus->getPoint();
        }

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pVampire);
        bool bHitRoll = HitRoll::isSuccessMagic(pVampire, pSkillInfo, pSkillSlot, HitBonus);
        bool bTileCheck = canBurrow(pZone, x, y);
        bool bMoveModeCheck = pVampire->isWalking();
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_CASKET);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bTileCheck && bMoveModeCheck && !bEffected) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);


            // Sends the skill packets before removing the Vampire from the ground.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK3.setXY(x, y);
            _GCSkillToSelfOK3.setSkillType(SkillType);
            _GCSkillToSelfOK3.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK3, pVampire);

            //---------------------------------------------------------------
            // Remove the existing lasting magic effects.
            //---------------------------------------------------------------
            EffectManager* pEffectManager = pVampire->getEffectManager();
            Assert(pEffectManager != NULL);

            Effect* pCheckEffect = NULL;

            // Get the user level.
            int userLevel = pVampire->getLevel();

            Effect::EffectClass effectClass;

            // EFFECT_CLASS_FLARE
            effectClass = Effect::EFFECT_CLASS_FLARE;
            pCheckEffect = pEffectManager->findEffect(effectClass);
            if (pCheckEffect != NULL) {
                int level = (dynamic_cast<EffectFlare*>(pCheckEffect))->getLevel();

                if (level < userLevel) {
                    pEffectManager->deleteEffect(effectClass);
                    pVampire->removeFlag(effectClass);
                }
            }

            // EFFECT_CLASS_DOOM
            effectClass = Effect::EFFECT_CLASS_DOOM;
            pCheckEffect = pEffectManager->findEffect(effectClass);
            if (pCheckEffect != NULL) {
                int level = (dynamic_cast<EffectDoom*>(pCheckEffect))->getLevel();

                if (level < userLevel) {
                    pEffectManager->deleteEffect(effectClass);
                    pVampire->removeFlag(effectClass);
                }
            }

            // EFFECT_CLASS_SEDUCTION
            effectClass = Effect::EFFECT_CLASS_SEDUCTION;
            pCheckEffect = pEffectManager->findEffect(effectClass);
            if (pCheckEffect != NULL) {
                int level = (dynamic_cast<EffectSeduction*>(pCheckEffect))->getLevel();

                if (level < userLevel) {
                    pEffectManager->deleteEffect(effectClass);
                    pVampire->removeFlag(effectClass);
                }
            }


            //---------------------------------------------------------------
            // Create the effect object and attach it.
            //---------------------------------------------------------------
            EffectSummonCasket* pEffect = new EffectSummonCasket(pVampire);
            pEffect->setDeadline(99999999);
            pEffect->setType(0);
            pVampire->addEffect(pEffect);
            pVampire->setFlag(Effect::EFFECT_CLASS_CASKET);

            // Apply the stat changes caused by SUMMON CASKET.
            VAMPIRE_RECORD prev;

            pVampire->getVampireRecord(prev);
            pVampire->initAllStat();
            pVampire->sendRealWearingInfo();
            pVampire->sendModifyInfo(prev);

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(pVampire->getObjectID());
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_CASKET);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcAddEffect, pVampire);

            pSkillSlot->setRunTime();
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Monster self handler
//////////////////////////////////////////////////////////////////////////////
void SummonCasket::execute(Monster* pMonster)

{}

SummonCasket g_SummonCasket;
