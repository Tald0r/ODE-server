////////////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : TurnUndead.cpp
////////////////////////////////////////////////////////////////////////////////

#include "TurnUndead.h"

#include "GCModifyInformation.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "RankBonus.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void TurnUndead::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    // Slayer Object Assertion
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Player* pPlayer = pSlayer->getPlayer();
        Assert(pPlayer != NULL);

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange()) && checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            // calculate damage and duration time
            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            bool bHit = false;

            Level_t maxEnemyLevel = 0;
            uint EnemyNum = 0;

            int oX, oY;

            for (oX = -2; oX <= 2; oX++)
                for (oY = -2; oY <= 2; oY++) {
                    int tileX = X + oX;
                    int tileY = Y + oY;

                    if (oX == 0 && oY == 0)
                        continue;
                    if (!rect.ptInRect(tileX, tileY))
                        continue;

                    Tile& tile = pZone->getTile(tileX, tileY);

                    // Builds a list of the creatures on the tile.
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

                        if (checkZoneLevelToHitTarget(pTargetCreature) && !pTargetCreature->isSlayer() &&
                            !pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA) &&
                            canAttack(pSlayer, pTargetCreature)) {
                            if (pTargetCreature->isVampire() || pTargetCreature->isOusters()) {
                                Player* pTargetPlayer = pTargetCreature->getPlayer();

                                bHit = true;

                                // Applies the damage.
                                GCModifyInformation gcMI;
                                ::setDamage(pTargetCreature, output.Damage, pSlayer, pSkillSlot->getSkillType(), &gcMI);

                                // Tells the target that its HP changed.
                                pTargetPlayer->sendPacket(&gcMI);

                                GCSkillToObjectOK2 gcSkillToObjectOK2;
                                gcSkillToObjectOK2.setObjectID(1); // Unused
                                gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                                gcSkillToObjectOK2.setDuration(0);
                            } else if (pTargetCreature->isMonster()) {
                                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                                bHit = true;

                                ::setDamage(pMonster, output.Damage, pSlayer, pSkillSlot->getSkillType());
                                pMonster->addEnemy(pSlayer);
                            } else {
                                continue;
                            }

                            if (maxEnemyLevel < pTargetCreature->getLevel())
                                maxEnemyLevel = pTargetCreature->getLevel();
                            EnemyNum++;

                            GCSkillToObjectOK4 gcSkillToObjectOK4;
                            gcSkillToObjectOK4.setTargetObjectID(pTargetCreature->getObjectID());
                            gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                            gcSkillToObjectOK4.setDuration(0);
                            pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(),
                                                   &gcSkillToObjectOK4);

                            // Raises the alignment.
                            increaseAlignment(pSlayer, pTargetCreature, _GCSkillToSelfOK1);
                        }
                    }
                }

            if (bHit) {
                shareAttrExp(pSlayer, output.Damage, 1, 1, 8, _GCSkillToSelfOK1);
                increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1, maxEnemyLevel,
                                  EnemyNum);
                increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);
            }

            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send the packet to the skill user.
            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);

            // Set the skill delay.
            pSkillSlot->setRunTime(output.Delay);

        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

TurnUndead g_TurnUndead;
