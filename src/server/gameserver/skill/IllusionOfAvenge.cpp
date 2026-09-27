////////////////////////////////////////////////////////////////////////////////
// Project     : DARKEDEN
// Module      : Skill - Effect
// File Name   : IllusionOfAvenge.cpp
////////////////////////////////////////////////////////////////////////////////

#include "IllusionOfAvenge.h"

#include "GCModifyInformation.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "SkillUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void IllusionOfAvenge::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_TRY


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

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, X, Y, pSkillInfo->getRange()) && checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessIllusionOfAvenge(pSlayer);

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        // calculate damage and duration time
        SkillInput input(pSlayer, pSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);


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

                        if (!pTargetCreature->isSlayer() && !pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                            if (pTargetCreature->isVampire()) {
                                Player* pTargetPlayer = pTargetCreature->getPlayer();


                                Vampire* pVampire = dynamic_cast<Vampire*>(pTargetCreature);

                                // Applies the damage.
                                GCModifyInformation gcMI;
                                ::setDamage(pVampire, output.Damage, pSlayer, pSkillSlot->getSkillType(), &gcMI);

                                // Tells the target that its HP changed.
                                pTargetPlayer->sendPacket(&gcMI);

                                GCSkillToObjectOK2 gcSkillToObjectOK2;
                                gcSkillToObjectOK2.setObjectID(1); // Unused
                                gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                                gcSkillToObjectOK2.setDuration(14);
                            } else if (pTargetCreature->isMonster()) {
                                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);

                                ::setDamage(pMonster, output.Damage, pSlayer, pSkillSlot->getSkillType());
                            }

                            GCSkillToObjectOK4 gcSkillToObjectOK4;
                            gcSkillToObjectOK4.setTargetObjectID(pTargetCreature->getObjectID());
                            gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                            gcSkillToObjectOK4.setDuration(14);
                            pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(),
                                                   &gcSkillToObjectOK4);

                            // Raises the alignment.
                            increaseAlignment(pSlayer, pTargetCreature, _GCSkillToSelfOK1);
                        }
                    }
                }


            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(14);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(14);

            // Send the packet to the skill user.
            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }

        // Skill delay setting
        if (bTimeCheck)
            pSkillSlot->setRunTime(output.Delay);
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

IllusionOfAvenge g_IllusionOfAvenge;
