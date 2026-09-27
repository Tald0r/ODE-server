//////////////////////////////////////////////////////////////////////////////
// Filename    : Rebuke.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Rebuke.h"

#include "EffectSleep.h"
#include "GCAddEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void Rebuke::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        if (bManaCheck && bTimeCheck && bRangeCheck) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // Compute the duration.
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            bool bHit = false;

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            for (int ox = -2; ox <= 2; ox++)
                for (int oy = -2; oy <= 2; oy++) {
                    int tileX = X + ox;
                    int tileY = Y + oy;

                    if (!rect.ptInRect(tileX, tileY))
                        continue;

                    Tile& tile = pZone->getTile(tileX, tileY);
                    if (tile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION))
                        continue;

                    list<Creature*> targetList;
                    if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);
                        targetList.push_back(pCreature);
                    }
                    if (tile.hasCreature(Creature::MOVE_MODE_FLYING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_FLYING);
                        targetList.push_back(pCreature);
                    }
                    if (tile.hasCreature(Creature::MOVE_MODE_BURROWING)) {
                        Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_BURROWING);
                        targetList.push_back(pCreature);
                    }

                    list<Creature*>::iterator itr = targetList.begin();
                    for (; itr != targetList.end(); itr++) {
                        Creature* pTargetCreature = (*itr);
                        Assert(pTargetCreature != NULL);

                        // Safe zone check
                        // 2003.1.10 by bezz, Sequoia
                        // NPCs are checked too
                        // 2003.3.14 by Sequoia
                        if (pTargetCreature->isNPC() || !checkZoneLevelToHitTarget(pTargetCreature) ||
                            !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA))
                            continue;

                        if (HitRoll::isSuccessRebuke(pSlayer, pSkillSlot, pTargetCreature)) {
                            // Damages the target that was hit.
                            if (pTargetCreature->isVampire()) {
                                GCModifyInformation gcMI;
                                setDamage(pTargetCreature, output.Damage, pSlayer, SkillType, &gcMI);

                                pTargetCreature->getPlayer()->sendPacket(&gcMI);
                            } else if (pTargetCreature->isMonster()) {
                                setDamage(pTargetCreature, output.Damage, pSlayer, SkillType);
                            }

                            // Deletes the existing SLEEP effect if the target already has one.
                            if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_SLEEP)) {
                                pTargetCreature->deleteEffect(Effect::EFFECT_CLASS_SLEEP);
                            }

                            EffectSleep* pEffect = new EffectSleep(pTargetCreature);

                            // Puts the SLEEP effect on the target that was hit.
                            pEffect->setDeadline(output.Duration);
                            pTargetCreature->addEffect(pEffect);
                            pTargetCreature->setFlag(Effect::EFFECT_CLASS_SLEEP);

                            GCAddEffect gcAddEffect;
                            gcAddEffect.setObjectID(pTargetCreature->getObjectID());
                            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_SLEEP);
                            gcAddEffect.setDuration(output.Duration);
                            pZone->broadcastPacket(tileX, tileY, &gcAddEffect);

                            bHit = true;

                            // Remembers the highest enemy level and how many
                            // enemies were hit; a Slayer target is not counted.
                            if (!pTargetCreature->isSlayer()) {
                                if (maxEnemyLevel < pTargetCreature->getLevel())
                                    maxEnemyLevel = pTargetCreature->getLevel();
                                EnemyNum++;
                            }
                        }
                    }
                }

            // Raises experience.
            if (bHit) {
                SkillGrade Grade =
                    de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
                Exp_t ExpUp = 10 * (Grade + 1);
                shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToSelfOK1);
                increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1, maxEnemyLevel,
                                  EnemyNum);
                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);
            }

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

Rebuke g_Rebuke;
