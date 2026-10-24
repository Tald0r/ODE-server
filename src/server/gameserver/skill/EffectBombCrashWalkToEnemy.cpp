
//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBombCrashWalkToEnemy.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBombCrashWalkToEnemy.h"

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
EffectBombCrashWalkToEnemy::EffectBombCrashWalkToEnemy(Creature* pCreature)

{
    m_UserObjectID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalkToEnemy::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalkToEnemy::affect(Creature* pCreature)

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
                    }
                }

                if (pAttacker->isPC())
                    pAttacker->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }
    // If the target is dead, end the effect.
    if (pCreature->isDead()) {
        setDeadline(0);
        setNextTime(0);
    }
    setNextTime(m_Tick);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalkToEnemy::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_SKILL_BOMB_CRASH_WALK_TO_ENEMY);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_SKILL_BOMB_CRASH_WALK_TO_ENEMY);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBombCrashWalkToEnemy::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBombCrashWalkToEnemy::toString() const {
    StringStream msg;
    msg << "EffectBombCrashWalkToEnemy(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
