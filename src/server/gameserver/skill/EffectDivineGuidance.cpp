
//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDivineGuidance.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDivineGuidance.h"

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDivineGuidance::EffectDivineGuidance(Creature* pCreature)

{
    m_UserObjectID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDivineGuidance::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDivineGuidance::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Creature* pAttacker = pZone->getCreature(m_UserObjectID);

    if (!(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE) && !pCreature->isDead() &&
        !pCreature->isFlag(Effect::EFFECT_CLASS_COMA)
        // Invincibility check.
        && canAttack(pAttacker, pCreature)) {
        GCModifyInformation gcMI, gcAttackerMI;
        setDamage(pCreature, m_Point, pAttacker, SKILL_DIVINE_GUIDANCE, &gcMI, &gcAttackerMI);
        if (pCreature->isPC())
            pCreature->getPlayer()->sendPacket(&gcMI);

        if (pAttacker != NULL) {
            computeAlignmentChange(pCreature, m_Point, pAttacker, &gcMI, &gcAttackerMI);
            if (pAttacker->isPC()) {
                if (pAttacker->isSlayer() && !pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pAttacker);

                    if (pSlayer != NULL) {
                        GCModifyInformation gcMI;
                        shareAttrExp(pSlayer, m_Point, 1, 1, 8, gcAttackerMI);
                    }
                }

                if (pAttacker->isPC())
                    pAttacker->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }

    setNextTime(m_Tick);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDivineGuidance::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_DIVINE_GUIDANCE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DIVINE_GUIDANCE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDivineGuidance::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectDivineGuidance::toString() const {
    StringStream msg;
    msg << "EffectDivineGuidance(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
