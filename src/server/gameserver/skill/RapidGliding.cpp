//////////////////////////////////////////////////////////////////////////////
// Filename    : RapidGliding.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "RapidGliding.h"

#include "GCModifyInformation.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK5.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire tile handler
//////////////////////////////////////////////////////////////////////////////
void RapidGliding::execute(Vampire* pVampire, ZoneCoord_t X, ZoneCoord_t Y, VampireSkillSlot* pVampireSkillSlot,
                           CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pVampireSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        SkillType_t SkillType = pVampireSkillSlot->getSkillType();

        // A missing target fails the skill instead of throwing.
        // An NPC cannot be attacked.

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK5 _GCSkillToTileOK5;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
        if (!rect.ptInRect(X, Y)) {
            executeSkillFailException(pVampire, getSkillType());
        }

        SkillInput input(pVampire);
        SkillOutput output;
        computeOutput(input, output);

        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pVampireSkillSlot);
        bool bRangeCheck = verifyDistance(pVampire, X, Y, output.Range);
        bool bEffected = pVampire->hasRelicItem() || pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);
        bool bPassLine = isPassLine(pZone, pVampire->getX(), pVampire->getY(), X, Y);

        if (bManaCheck && bTimeCheck && bRangeCheck && !bEffected && bPassLine) {
            // Moves the PC quickly.
            if (pZone->moveFastPC(pVampire, pVampire->getX(), pVampire->getY(), X, Y, getSkillType())) {
                decreaseMana(pVampire, RequiredMP, _GCSkillToTileOK1);

                _GCSkillToTileOK1.setSkillType(SkillType);
                _GCSkillToTileOK1.setCEffectID(0);
                _GCSkillToTileOK1.setX(X);
                _GCSkillToTileOK1.setY(Y);
                _GCSkillToTileOK1.setRange(0);
                _GCSkillToTileOK1.setDuration(0);

                _GCSkillToTileOK5.setObjectID(pVampire->getObjectID());
                _GCSkillToTileOK5.setSkillType(SkillType);
                _GCSkillToTileOK5.setX(X);
                _GCSkillToTileOK5.setY(Y);
                _GCSkillToTileOK5.setRange(0);
                _GCSkillToTileOK5.setDuration(0);

                // Tells the player their own HP changed.
                pPlayer->sendPacket(&_GCSkillToTileOK1);

                // Tells nearby creatures that the HP changed.
                GCStatusCurrentHP gcStatusCurrentHP;
                gcStatusCurrentHP.setObjectID(pVampire->getObjectID());
                gcStatusCurrentHP.setCurrentHP(pVampire->getHP());
                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcStatusCurrentHP, pVampire);
                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &_GCSkillToTileOK5, pVampire);

                pVampireSkillSlot->setRunTime(output.Delay);
            } else {
                executeSkillFailException(pVampire, getSkillType());
            }
        } else {
            executeSkillFailException(pVampire, getSkillType());
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}
//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void RapidGliding::execute(Vampire* pVampire, ObjectID_t TargetObjectID, VampireSkillSlot* pVampireSkillSlot,
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
        // An NPC cannot be attacked.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }


        execute(pVampire, pTargetCreature->getX(), pTargetCreature->getY(), pVampireSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

RapidGliding g_RapidGliding;
