#include "EffectDestructionSpear.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Ousters.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SkillUtil.h"
#include "Zone.h"

EffectDestructionSpear::EffectDestructionSpear(Creature* pCreature) {
    m_pTarget = pCreature;
    m_CanSteal = false;
}

void EffectDestructionSpear::affect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        affect(dynamic_cast<Creature*>(m_pTarget));
    }

    setNextTime(10);
}

void EffectDestructionSpear::affect(Creature* pCreature) {
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

            ::setDamage(pPC, getDamage(), pCastCreature, SKILL_DESTRUCTION_SPEAR, &gcMI, &gcAttackerMI, true,
                        m_CanSteal);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            ::setDamage(pCreature, getDamage(), pCastCreature, SKILL_DESTRUCTION_SPEAR, NULL, &gcAttackerMI, true,
                        m_CanSteal);
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

void EffectDestructionSpear::unaffect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        unaffect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectDestructionSpear::unaffect(Creature* pCreature) {
    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

string EffectDestructionSpear::toString() const {
    return "EffectDestructionSpear";
}
