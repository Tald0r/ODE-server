//////////////////////////////////////////////////////////////////////////////
// Filename    : InstallTrap.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "InstallTrap.h"

#include "EffectTrapInstalled.h"
#include "GCAddEffectToTile.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "ItemUtil.h"
#include "Tile.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self
//////////////////////////////////////////////////////////////////////////////
void InstallTrap::execute(Slayer* pSlayer, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // The skill cannot be used if no weapon is equipped or it is not a sword.
        Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pWeapon == NULL || !isArmsWeapon(pWeapon)) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }


        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t X = pSlayer->getX();
        ZoneCoord_t Y = pSlayer->getY();
        Tile& rTile = pZone->getTile(X, Y);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bHitRoll = HitRoll::isSuccessMagic(pSlayer, pSkillInfo, pSkillSlot);
        bool bEffected = rTile.getEffect(Effect::EFFECT_CLASS_TRAP_INSTALLED) != NULL ||
                         rTile.getEffect(Effect::EFFECT_CLASS_TRYING_POSITION) != NULL;

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && !bEffected) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the effect class and attach it.
            EffectTrapInstalled* pEffect = new EffectTrapInstalled(pZone, X, Y);
            pEffect->setDeadline(output.Duration);
            pEffect->setUserOID(pSlayer->getObjectID());
            pEffect->setTick(output.Tick);

            pZone->registerObject(pEffect);
            pZone->addEffect(pEffect);
            rTile.addEffect(pEffect);

            // Raises experience.

            // Build the packet and send it.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(output.Duration);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(output.Duration);

            GCAddEffectToTile gcAddEffect;
            gcAddEffect.setXY(X, Y);
            gcAddEffect.setEffectID(Effect::EFFECT_CLASS_TRAP_INSTALLED);
            gcAddEffect.setObjectID(pEffect->getObjectID());
            gcAddEffect.setDuration(output.Duration);
            pZone->broadcastPacket(X, Y, &gcAddEffect);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);
            pZone->broadcastPacket(X, Y, &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

InstallTrap g_InstallTrap;
