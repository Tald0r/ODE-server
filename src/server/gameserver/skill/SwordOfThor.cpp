//////////////////////////////////////////////////////////////////////////////
// Filename    : SwordOfThor.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SwordOfThor.h"

#include <list>

#include "EffectSwordOfThor.h"
#include "GCAddEffectToTile.h"
#include "SimpleMeleeSkill.h"
#include "SimpleTileMissileSkill.h"
#include "ZoneUtil.h"

void SwordOfThor::execute(Slayer* pSlayer, ObjectID_t targetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID) {
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    Creature* pTargetCreature = pZone->getCreature(targetObjectID);

    if (pTargetCreature == NULL) {
        executeSkillFailException(pSlayer, getSkillType());
        return;
    }

    execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
}

void SwordOfThor::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_SWORD;
    param.STRMultiplier = 8;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 1;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = false;
    param.bExpForTotalDamage = false;

    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j) {
            param.addMask(i, j, 100);
        }

    SIMPLE_SKILL_OUTPUT result;

    // The target tile and the area around it.

    g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);

    if (result.bSuccess) {
        for (int i = -2; i <= 2; ++i)
            for (int j = -2; j <= 2; ++j) {
                ZoneCoord_t tx = X + i;
                ZoneCoord_t ty = Y + j;

                if (!isValidZoneCoord(pZone, tx, ty))
                    continue;
                Tile& rTile = pZone->getTile(tx, ty);
                if (!rTile.canAddEffect())
                    continue;

                EffectSwordOfThor* pEffect = new EffectSwordOfThor(pZone, tx, ty);
                pEffect->setLevel(input.SkillLevel);
                pEffect->setDeadline(output.Duration);
                pZone->registerObject(pEffect);

                if (i != 0 || j != 0)
                    pEffect->setBroadcastingEffect(false);
                else {
                    GCAddEffectToTile gcAE;
                    gcAE.setEffectID(pEffect->getSendEffectClass());
                    gcAE.setXY(tx, ty);
                    gcAE.setObjectID(pEffect->getObjectID());
                    gcAE.setDuration(output.Duration);
                    pZone->broadcastPacket(tx, ty, &gcAE);
                }

                pZone->addEffect(pEffect);
                rTile.addEffect(pEffect);
            }
    }
}

SwordOfThor g_SwordOfThor;
