//////////////////////////////////////////////////////////////////////////////
// Filename    : Flare.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Flare.h"

#include "EffectFlare.h"
#include "GCAddEffect.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK3.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToObjectOK5.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Flare::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // Flare cannot be used on an NPC or a Slayer.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || pTargetCreature->isNPC() || pTargetCreature->isSlayer()) {
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
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = HitRoll::isSuccessFlare(pTargetCreature, SkillLevel) && canAttack(pSlayer, pTargetCreature);
        bool bEffected = pTargetCreature->isFlag(Effect::EFFECT_CLASS_FLARE);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected &&
            pTargetCreature->getCompetence() == 3) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

            // Compute the duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectFlare* pEffect = new EffectFlare(pTargetCreature);
            pEffect->setOldSight(pTargetCreature->getSight());
            // Keeps the level for the check made when the effect is removed.
            pEffect->setLevel(pSkillInfo->getLevel());
            pEffect->setDeadline(output.Duration);
            pTargetCreature->setFlag(Effect::EFFECT_CLASS_FLARE);
            pTargetCreature->addEffect(pEffect);

            // Apply the effect.
            pEffect->affect(pTargetCreature);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToObjectOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);

            // Prepare the packet.
            ZoneCoord_t targetX = pTargetCreature->getX();
            ZoneCoord_t targetY = pTargetCreature->getY();
            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);

            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.addShortData(MODIFY_VISION, FLARE_SIGHT);
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK3.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK3.setSkillType(SkillType);
            _GCSkillToObjectOK3.setTargetXY(targetX, targetY);

            _GCSkillToObjectOK4.setSkillType(SkillType);
            _GCSkillToObjectOK4.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK4.setDuration(0);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(0);

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pSlayer);
            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCSkillToObjectOK5, cList);
            pZone->broadcastPacket(myX, myY, &_GCSkillToObjectOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCSkillToObjectOK4, cList);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pSlayer);
            }

            GCAddEffect gcAddEffect;
            gcAddEffect.setObjectID(TargetObjectID);
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_FLARE);
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(targetX, targetY, &gcAddEffect);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

Flare g_Flare;
