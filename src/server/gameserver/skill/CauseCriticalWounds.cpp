//////////////////////////////////////////////////////////////////////////////
// Filename    : CauseCriticalWounds.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CauseCriticalWounds.h"

#include "EffectCauseCriticalWounds.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCSkillToObjectOK6.h"
#include "GameContext.h"
#include "RankBonus.h"
#include "SimpleMissileSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void CauseCriticalWounds::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot,
                                  CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // NPCs cannot be attacked.
        // A target immune to paralyze is rejected.
        // A missing target is rejected too.
        if (pTargetCreature == NULL || pTargetCreature->isFlag(Effect::EFFECT_CLASS_IMMUNE_TO_PARALYZE) ||
            !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());

            return;
        }

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        Tile& rTile = pZone->getTile(pTargetCreature->getX(), pTargetCreature->getY());

        // Check whether the skill can be used.
        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bCanHit = canHit(pSlayer, pTargetCreature, SkillType);
        bool bEffected = pTargetCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
                         rTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL;
        bool bPK = verifyPK(pSlayer, pTargetCreature);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanHit && !bEffected && bPK) {
            // packet
            GCSkillToObjectOK1 _GCSkillToObjectOK1;
            GCSkillToObjectOK2 _GCSkillToObjectOK2;
            GCSkillToObjectOK3 _GCSkillToObjectOK3;
            GCSkillToObjectOK4 _GCSkillToObjectOK4;
            GCSkillToObjectOK5 _GCSkillToObjectOK5;
            GCSkillToObjectOK6 _GCSkillToObjectOK6;

            ZoneCoord_t prayerX = pSlayer->getX();
            ZoneCoord_t prayerY = pSlayer->getY();
            ZoneCoord_t targetX = pTargetCreature->getX();
            ZoneCoord_t targetY = pTargetCreature->getY();

            // Compute the skill's damage, delay, and duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Soul Smashing adds its rank bonus to the damage.
            if (pSlayer->hasRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING)) {
                RankBonus* pRankBonus = pSlayer->getRankBonus(RankBonus::RANK_BONUS_SOUL_SMASHING);
                Assert(pRankBonus != NULL);

                output.Damage += pRankBonus->getPoint();
            }

            // Decrease the mana.
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            // Deal the damage and drop durability; alignment is adjusted along with it.
            setDamage(pTargetCreature, output.Damage, pSlayer, SkillType, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
            computeAlignmentChange(pTargetCreature, output.Damage, pSlayer, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
            decreaseDurability(pSlayer, pTargetCreature, NULL, &_GCSkillToObjectOK1, &_GCSkillToObjectOK2);

            // Only when the target is not a Slayer:
            // raise the experience,
            // attach the effect,
            // and broadcast the effect.
            if (!pTargetCreature->isSlayer()) {
                SkillGrade Grade =
                    de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
                Exp_t ExpUp = 10 * (Grade + 1);
                shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToObjectOK1);
                increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1,
                                  pTargetCreature->getLevel());
                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);
                increaseAlignment(pSlayer, pTargetCreature, _GCSkillToObjectOK1);

                // Create the effect and attach it.
                EffectCauseCriticalWounds* pEffectCauseCriticalWounds = new EffectCauseCriticalWounds(pTargetCreature);
                pEffectCauseCriticalWounds->setDeadline(output.Duration);
                pTargetCreature->addEffect(pEffectCauseCriticalWounds);
                pTargetCreature->setFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS);

                // Broadcast the effect.
                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(pTargetCreature->getObjectID());
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS);
                gcAddEffect.setDuration(output.Duration);
                pZone->broadcastPacket(targetX, targetY, &gcAddEffect);
            } else // The target is a Slayer.
            {
                // A Slayer target gets duration 0: no paralysis, damage only.
                output.Duration = 0;
            }

            bool bCanSeePrayer = canSee(pTargetCreature, pSlayer);

            // For the skill user
            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(output.Duration);

            // For the skill's target
            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(output.Duration);

            // For those who can see only the skill user
            _GCSkillToObjectOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            // For those who can see only the target
            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);

            // For those who can see both the skill user and the target
            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setDuration(output.Duration);

            // For when the target cannot see the skill user
            _GCSkillToObjectOK6.setXY(prayerX, prayerY);
            _GCSkillToObjectOK6.setSkillType(SkillType);
            _GCSkillToObjectOK6.setDuration(output.Duration);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);

                if (bCanSeePrayer)
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
                else
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK6);
            } else {
                Monster* pTargetMonster = dynamic_cast<Monster*>(pTargetCreature);
                Assert(pTargetMonster != NULL);

                pTargetMonster->addEnemy(pSlayer);
            }

            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList.push_back(pTargetCreature);

            cList = pZone->broadcastSkillPacket(prayerX, prayerY, targetX, targetY, &_GCSkillToObjectOK5, cList);

            pZone->broadcastPacket(prayerX, prayerY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

CauseCriticalWounds g_CauseCriticalWounds;
