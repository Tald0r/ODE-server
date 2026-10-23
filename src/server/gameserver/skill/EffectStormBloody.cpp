//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectStormBloody.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectStormBloody.h"

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Ousters.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectStormBloody::EffectStormBloody(Creature* pCreature)

{
    m_UserObjectID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Get the user.
    // It may be NULL: the creature may already have left the zone.
    // by bezz. 2003.3.13
    Creature* pCastCreature = pZone->getCreature(m_UserObjectID);
    // Ignore it if the caster is gone.
    if (pCastCreature == NULL)
        return;

    // EffectStormBloody is attached when walking over AcidStorm, PoisonStorm or BloodyStorm.
    // It deals damage three times in a row and then disappears.

    Damage_t StormDamage = m_Point;
    GCModifyInformation GCAttackerMI;

    if (!(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)
        // Invincibility check.
        && canAttack(pCastCreature, pCreature)) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pSlayer, StormDamage, pCastCreature, SKILL_BLOODY_STORM, &gcMI, &GCAttackerMI);

            pSlayer->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pVampire, StormDamage, pCastCreature, SKILL_BLOODY_STORM, &gcMI, &GCAttackerMI);

            pVampire->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pOusters, StormDamage, pCastCreature, SKILL_BLOODY_STORM, &gcMI, &GCAttackerMI);

            pOusters->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            setDamage(pMonster, StormDamage, pCastCreature, SKILL_BLOODY_STORM, NULL, &GCAttackerMI);
        }

        if (pCastCreature->isVampire() && pCreature->isDead()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCastCreature);
            int exp = computeCreatureExp(pCreature, KILL_EXP);
            shareVampExp(pVampire, exp, GCAttackerMI);
            pVampire->getPlayer()->sendPacket(&GCAttackerMI);
        }

        // Handles the kill count when the caster kills pCreature.
        // by sigi. 2002.9.9
        // Handled by calling setDamage instead.
        // by bezz. 2002.12.31
    }

    setNextTime(m_Tick);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    pCreature->removeFlag(Effect::EFFECT_CLASS_STORM_BLOODY);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_STORM_BLOODY);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectStormBloody::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectStormBloody::toString() const {
    StringStream msg;
    msg << "EffectStormBloody(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
