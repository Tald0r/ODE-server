//////////////////////////////////////////////////////////////////////////////
// Filename    : Destinies.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Destinies.h"

#include "EffectDestinies.h"
#include "SimpleMissileSkill.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Vampire object handler
//////////////////////////////////////////////////////////////////////////////
void Destinies::execute(Ousters* pOusters, ObjectID_t TargetObjectID, OustersSkillSlot* pOustersSkillSlot,
                        CEffectID_t CEffectID)

{
    SkillInput input(pOusters, pOustersSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_OUSTERS_WRISTLET;
    param.STRMultiplier = 0;
    param.DEXMultiplier = 0;
    param.INTMultiplier = 0;
    param.bMagicHitRoll = false;
    param.bMagicDamage = true;
    param.bAdd = false;

    SIMPLE_SKILL_OUTPUT result;

    g_SimpleMissileSkill.execute(pOusters, TargetObjectID, pOustersSkillSlot, param, result, CEffectID);

    if (result.bSuccess) {
        cout << "Destinies succeeded" << endl;
        if (result.pTargetCreature->isOusters())
            return;

        EffectDestinies* pEffect = new EffectDestinies(pOusters);
        pEffect->setTargetType(result.pTargetCreature);
        pEffect->setDamage(output.Damage);
        pEffect->setNextTime(10);
        pEffect->setBroadcastingEffect(false);

        pOusters->addEffect(pEffect);
    }
}

Destinies g_Destinies;
