//////////////////////////////////////////////////////////////////////////////
// Filename    : Uninvisibility.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Uninvisibility.h"

#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Uninvisibility::execute(Vampire* pVampire)

{
    Assert(pVampire != NULL);

    try {
        if (pVampire->isDead()) {
            return;
        }

        Zone* pZone = pVampire->getZone();
        Assert(pZone != NULL);


        if (!pVampire->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            executeSkillFailException(pVampire, getSkillType());
            return;
        }

        addVisibleCreature(pZone, pVampire, true);
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void Uninvisibility::execute(Monster* pMonster)

{
    Assert(pMonster != NULL);

    try {
        if (pMonster->isDead()) {
            return;
        }

        Zone* pZone = pMonster->getZone();
        Assert(pZone != NULL);

        if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pMonster, true);
        }
    } catch (Throwable& t) {
    }
}

Uninvisibility g_Uninvisibility;
