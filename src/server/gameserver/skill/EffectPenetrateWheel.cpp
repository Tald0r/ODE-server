//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectPenetrateWheel.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectPenetrateWheel.h"

#include "DB.h"
#include "Effect.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "HitRoll.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectPenetrateWheel::EffectPenetrateWheel(Creature* pCreature) {
    m_pTarget = pCreature;
    m_CanSteal = false;
}

void EffectPenetrateWheel::affect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        affect(dynamic_cast<Creature*>(m_pTarget));
    }

    setNextTime(10);
}

void EffectPenetrateWheel::affect(Creature* pCreature) {
    if (pCreature == NULL)
        return;

    Zone* pZone = pCreature->getZone();
    if (pZone == NULL)
        return;

    Creature* pCastCreature = pZone->getCreature(getCasterID());

    GCModifyInformation gcMI, gcAttackerMI;

    if (canAttack(pCastCreature, pCreature) && !(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)) {
        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            // There is no attacker. This is to ensure that the Sharp Shield is not applied and the Steel is not
            // applied.
            ::setDamage(pPC, getDamage(), NULL, SKILL_Penetrate_Wheel, &gcMI, &gcAttackerMI, false, m_CanSteal);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            // There is no attacker. This is to ensure that the Sharp Shield is not applied and the Steel is not
            // applied.
            ::setDamage(pCreature, getDamage(), NULL, SKILL_Penetrate_Wheel, NULL, &gcAttackerMI, false, m_CanSteal);
        }

        if (pCastCreature != NULL && pCastCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCastCreature);

            computeAlignmentChange(pCreature, getDamage(), pOusters, &gcMI, &gcAttackerMI);
            increaseAlignment(pOusters, pCreature, gcAttackerMI);

            if (pCreature->isDead()) {
                int exp = computeCreatureExp(pCreature, 100, pOusters);
                shareOustersExp(pOusters, exp, gcAttackerMI);
            }

            pOusters->getPlayer()->sendPacket(&gcAttackerMI);
        }
    }

    setNextTime(20);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectPenetrateWheel::unaffect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        unaffect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectPenetrateWheel::unaffect(Creature* pCreature) {
    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectPenetrateWheel::toString() const {
    StringStream msg;

    msg << "EffectPenetrateWheel(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
