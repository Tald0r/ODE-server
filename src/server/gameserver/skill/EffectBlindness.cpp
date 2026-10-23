#include "EffectBlindness.h"

#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Ousters.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SkillUtil.h"
#include "Zone.h"

EffectBlindness::EffectBlindness(Creature* pCreature) {
    m_pTarget = pCreature;
}

void EffectBlindness::affect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        affect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectBlindness::affect(Creature* pCreature) {
    if (pCreature == NULL)
        return;

    Zone* pZone = pCreature->getZone();
    if (pZone == NULL)
        return;

    GCModifyInformation gcMI;

    if (!pCreature->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE) && !(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)) {
        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            ::setDamage(pPC, getDamage(), NULL, SKILL_ATTACK_MELEE, &gcMI, NULL);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            ::setDamage(pCreature, getDamage(), NULL, SKILL_ATTACK_MELEE, NULL, NULL);
        }
    }

    setNextTime(30);
}

void EffectBlindness::unaffect() {
    if (m_pTarget != NULL && m_pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        unaffect(dynamic_cast<Creature*>(m_pTarget));
    }
}

void EffectBlindness::unaffect(Creature* pCreature) {
    pCreature->removeFlag(getEffectClass());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Tells clients that the effect is gone.
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(getSendEffectClass());
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

string EffectBlindness::toString() const {
    return "EffectBlindness";
}
