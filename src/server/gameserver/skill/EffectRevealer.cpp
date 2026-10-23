//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRevealer.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectRevealer.h"

#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRevealer::EffectRevealer(Creature* pCreature)

{
    // Only a Slayer can use this effect.
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRevealer::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRevealer::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRevealer::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    // Removes the flag.
    pCreature->removeFlag(Effect::EFFECT_CLASS_REVEALER);

    // Removes the creatures that were visible through the magic.
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);
    pZone->updateMineScan(pCreature);
    // Hidden creatures are scanned as well.
    pZone->updateHiddenScan(pCreature);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_REVEALER);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRevealer::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRevealer::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectRevealer::toString() const {
    StringStream msg;
    msg << "EffectRevealer(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}

bool EffectRevealer::canSeeHide(Creature* pTarget) const

{
    if (!pTarget->isFlag(Effect::EFFECT_CLASS_HIDE))
        return true;

    Level_t level;

    if (pTarget->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pTarget);
        Assert(pVampire != NULL);

        level = pVampire->getLevel();
    } else if (pTarget->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pTarget);
        Assert(pMonster != NULL);

        level = pMonster->getLevel();
    } else {
        throw Error("Hide is active on a creature that is neither a vampire nor a monster");
    }

    if (m_SkillLevel >= 25 && m_SkillLevel > level)
        return true;
    else
        return false;
}

bool EffectRevealer::canSeeSniping(Creature* pTarget) const

{
    if (!pTarget->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE))
        return true;

    Assert(pTarget->isSlayer());
    Slayer* pSlayer = dynamic_cast<Slayer*>(pTarget);

    ExpLevel_t level;

    SkillSlot* pSkillSlot = pSlayer->getSkill(SKILL_SNIPING);
    Assert(pSkillSlot != NULL);

    level = pSkillSlot->getExpLevel();

    if (m_SkillLevel >= 45 && m_SkillLevel > level)
        return true;
    else
        return false;
}

bool EffectRevealer::canSeeInvisibility(Creature* pTarget) const

{
    if (!pTarget->isFlag(Effect::EFFECT_CLASS_INVISIBILITY))
        return true;

    Level_t level;

    if (pTarget->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pTarget);
        Assert(pVampire != NULL);

        level = pVampire->getLevel();
    } else if (pTarget->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pTarget);
        Assert(pMonster != NULL);

        level = pMonster->getLevel();
    } else {
        throw Error("Invisibility is active on a creature that is neither a vampire nor a monster.");
    }

    if (m_SkillLevel >= 65 && (m_SkillLevel - 20) >= level)
        return true;
    else
        return false;
}
