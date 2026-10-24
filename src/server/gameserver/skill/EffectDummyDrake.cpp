//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDummyDrake.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDummyDrake.h"

#include "DB.h"
#include "GCAddEffect.h"
#include "GCRemoveEffect.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Zone.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDummyDrake::EffectDummyDrake(Creature* pCreature)

{
    m_pTarget = pCreature;
    m_UserOID = 0;
    m_Damage = 0;
    m_Duration = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDummyDrake::affect()

{
    setNextTime(10);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDummyDrake::affect(Creature* pCreature)

{
    if (pCreature == NULL)
        return;

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    Ousters* pOusters = dynamic_cast<Ousters*>(pZone->getCreature(m_UserOID));

    GCModifyInformation gcMI, gcAttackerMI;

    if (canAttack(pOusters, pCreature) && !(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)) {
        Damage_t damage = getDamage();
        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            ::setDamage(pPC, damage, pOusters, SKILL_DUMMY_DRAKE, &gcMI, &gcAttackerMI, true, false);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            ::setDamage(pCreature, damage, pOusters, SKILL_DUMMY_DRAKE, NULL, &gcAttackerMI, true, false);
        }

        if (pOusters != NULL) {
            computeAlignmentChange(pCreature, damage, pOusters, &gcMI, &gcAttackerMI);
            increaseAlignment(pOusters, pCreature, gcAttackerMI);

            if (pCreature->isDead()) {
                int exp = computeCreatureExp(pCreature, 100, pOusters);
                shareOustersExp(pOusters, exp, gcAttackerMI);
            }

            pOusters->getPlayer()->sendPacket(&gcAttackerMI);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDummyDrake::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    pCreature->removeFlag(Effect::EFFECT_CLASS_DUMMY_DRAKE);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DUMMY_DRAKE);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDummyDrake::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    // Take the objects within a 5*5 range and damage them.
    // Damage the target.
    affect(pCreature);

    int cx = pCreature->getX();
    int cy = pCreature->getY();
    //
    Zone* pZone = pCreature->getZone();
    //
    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j) {
            int tx = cx + i;
            int ty = cy + j;
            if (tx < 0 || ty < 0)
                continue;
            if (!isValidZoneCoord(pZone, tx, ty))
                continue;

            forward_list<Object*>& olist = pZone->getTile(tx, ty).getObjectList();
            forward_list<Object*>::iterator itr = olist.begin();
            for (; itr != olist.end(); ++itr) {
                Object* pObject = *itr;
                if (pObject == NULL || pObject->getObjectClass() != Object::OBJECT_CLASS_CREATURE)
                    continue;

                Creature* pTargetCreature = dynamic_cast<Creature*>(pObject);
                if (pTargetCreature == NULL || pTargetCreature->isOusters() || pTargetCreature == pCreature)
                    continue;
                if (pTargetCreature->isFlag(getEffectClass()))
                    continue;
                // Apply the damage.
                affect(pTargetCreature);
            }
        }
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectDummyDrake::toString() const {
    StringStream msg;

    msg << "EffectDummyDrake(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
