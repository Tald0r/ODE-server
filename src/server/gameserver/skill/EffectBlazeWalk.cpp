//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBlazeWalk.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBlazeWalk.h"

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBlazeWalk::EffectBlazeWalk(Creature* pCreature)

{
    m_UserObjectID = 0;
    m_AttackNum = 0;
    setTarget(pCreature);
    m_SkillType = SKILL_BLAZE_WALK;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBlazeWalk::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBlazeWalk::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Creature* pAttacker = pZone->getCreature(m_UserObjectID);
    if (pAttacker == NULL) {
        setDeadline(0);
        return;
    }

    if (!(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE) && !pCreature->isDead() &&
        !pCreature->isFlag(Effect::EFFECT_CLASS_COMA)
        // Invincibility check.
        && canAttack(pAttacker, pCreature)) {
        GCModifyInformation gcMI, gcAttackerMI;
        setDamage(pCreature, m_Point, pAttacker, m_SkillType, &gcMI, &gcAttackerMI);

        if (pAttacker->isPC())
            computeAlignmentChange(pCreature, m_Point, pAttacker, &gcMI, &gcAttackerMI);
        if (pCreature->isPC())
            pCreature->getPlayer()->sendPacket(&gcMI);

        if (pAttacker->isSlayer() && !pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pAttacker);

            if (pSlayer != NULL) {
                GCModifyInformation gcMI;
                shareAttrExp(pSlayer, m_Point, 8, 1, 1, gcAttackerMI);
            }
        }

        if (pAttacker->isPC())
            pAttacker->getPlayer()->sendPacket(&gcAttackerMI);

        if (pCreature->isDead())
            setDeadline(0);
    }

    m_AttackNum--;

    if (m_AttackNum > 0) {
        setNextTime(5);
    } else
        setDeadline(0);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBlazeWalk::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_BLAZE_WALK);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBlazeWalk::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBlazeWalk::toString() const {
    StringStream msg;
    msg << "EffectBlazeWalk(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
