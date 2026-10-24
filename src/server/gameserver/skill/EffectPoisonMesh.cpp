#include "EffectPoisonMesh.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SkillUtil.h"
#include "Zone.h"

EffectPoisonMesh::EffectPoisonMesh(Creature* pCreature) {
    m_pTarget = pCreature;
    m_pZone = pCreature->getZone();
}

void EffectPoisonMesh::affect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        affect(dynamic_cast<Creature*>(m_pTarget));
    }

    setNextTime(10);
}

void EffectPoisonMesh::affect(Creature* pCreature) {
    if (pCreature == NULL)
        return;

    Creature* pCastCreature = m_pZone->getCreature(getCasterID());

    if (canAttack(pCastCreature, pCreature) && !(m_pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)) {
        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            GCModifyInformation gcMI;
            ::setDamage(pPC, getDamage(), pCastCreature, SKILL_POISON_MESH, &gcMI);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            ::setDamage(pCreature, getDamage(), pCastCreature, SKILL_POISON_MESH);
        }
    }
}

void EffectPoisonMesh::unaffect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        unaffect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectPoisonMesh::unaffect(Creature* pCreature) {
    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

string EffectPoisonMesh::toString() const {
    return "EffectPoisonMesh";
}
