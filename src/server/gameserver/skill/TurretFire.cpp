//////////////////////////////////////////////////////////////////////////////
// Filename    : TurretFire.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TurretFire.h"

#include "ItemUtil.h"
#include "SimpleMeleeSkill.h"
#include "SimpleTileMissileSkill.h"

void TurretFire::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    if (!pSlayer->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
        executeSkillFailException(pSlayer, getSkillType());
    }

    Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    if (pWeapon == NULL || isArmsWeapon(pWeapon) == false) {
        executeSkillFailException(pSlayer, getSkillType());
        return;
    }

    bool bBulletCheck = (getRemainBullet(pWeapon) > 0) ? true : false;

    // The bullet count always goes down.
    Bullet_t RemainBullet = 0;
    if (bBulletCheck) {
        // Lowers the bullet count, stores it and reads back the remaining count.
        decreaseBullet(pWeapon);
        RemainBullet = getRemainBullet(pWeapon);
    } else {
        executeSkillFailNormalWithGun(pSlayer, getSkillType(), NULL, RemainBullet);
        return;
    }

    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = 0xffffffff;
    param.ItemClass = Item::ITEM_CLASS_MAX;
    param.STRMultiplier = 1;
    param.DEXMultiplier = 8;
    param.INTMultiplier = 1;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    // The target tile and the area around it.
    param.addMask(0, 0, 100);
    param.addMask(-1, -1, 100);
    param.addMask(0, -1, 100);
    param.addMask(1, -1, 100);
    param.addMask(-1, 0, 100);
    param.addMask(1, 0, 100);
    param.addMask(-1, 1, 100);
    param.addMask(0, 1, 100);
    param.addMask(1, 1, 100);

    g_SimpleTileMissileSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void TurretFire::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

    if (pTargetCreature != NULL) {
        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    } else {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

TurretFire g_TurretFire;
