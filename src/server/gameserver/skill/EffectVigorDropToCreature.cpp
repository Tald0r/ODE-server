//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectVigorDropToCreature.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectVigorDropToCreature.h"

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
EffectVigorDropToCreature::EffectVigorDropToCreature(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectVigorDropToCreature::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectVigorDropToCreature::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Get the skill user.
    // It may be NULL: the creature may already have left the zone.
    // by bezz. 2003.1.4
    Creature* pCastCreature = pZone->getCreature(m_UserObjectID);

    // EffectVigorDropToCreature is attached when passing over AcidStorm, PoisonStorm or BloodyStorm.
    // It deals damage three times in a row and then disappears.

    Damage_t DropDamage = m_Point;

    if (!(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)
        // Invincibility check.
        && canAttack(pCastCreature, pCreature) && !pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pSlayer, DropDamage, pCastCreature, SKILL_VIGOR_DROP, &gcMI);

            pSlayer->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pVampire, DropDamage, pCastCreature, SKILL_VIGOR_DROP, &gcMI);

            pVampire->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            GCModifyInformation gcMI;
            setDamage(pOusters, DropDamage, pCastCreature, SKILL_VIGOR_DROP, &gcMI);

            pOusters->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            setDamage(pMonster, DropDamage, pCastCreature, SKILL_VIGOR_DROP);
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
void EffectVigorDropToCreature::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectVigorDropToCreature::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    pCreature->removeFlag(Effect::EFFECT_CLASS_VIGOR_DROP_TO_CREATURE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_VIGOR_DROP_TO_CREATURE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectVigorDropToCreature::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectVigorDropToCreature::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectVigorDropToCreature::toString() const {
    StringStream msg;
    msg << "EffectVigorDropToCreature(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
