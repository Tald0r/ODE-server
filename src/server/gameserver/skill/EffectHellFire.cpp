//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHellFire.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHellFire.h"

#include "DB.h"
#include "EffectHellFireToEnemy.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHellFire::EffectHellFire(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_CasterName = "";
    m_CasterID = 0;
    m_bForce = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFire::affect()

{
    Assert(m_pZone != NULL);

    // Fetches the creature that used the effect.
    // It may be NULL: the creature may already have left the zone.
    // by bezz. 2003.1.4
    Creature* pCastCreature = m_pZone->getCreature(m_CasterID);

    // Get the tile this effect is attached to.
    Tile& tile = m_pZone->getTile(m_X, m_Y);

    // Walk the objects on the tile.
    const forward_list<Object*>& oList = tile.getObjectList();
    forward_list<Object*>::const_iterator itr = oList.begin();
    for (; itr != oList.end(); itr++) {
        Assert(*itr != NULL);

        Object* pObject = *itr;
        Assert(pObject != NULL);

        if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
            Creature* pCreature = dynamic_cast<Creature*>(pObject);
            Assert(pCreature != NULL);

            // Check for invulnerability.
            // Acid immunity.
            // The caster itself is not hit.
            // Safe zone check.
            // 2003.1.10 by bezz, Sequoia
            if (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
                pCreature->getObjectID() == m_CasterID || !checkZoneLevelToHitTarget(pCreature)) {
                continue;
            }

            // A creature of the same class is not hit.
            if (pCreature->isOusters() && !isForce()) {
                continue;
            }

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING &&
                !pCreature->isFlag(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY)) {
                // Creates the effect class and attaches it.
                EffectHellFireToEnemy* pEffect = new EffectHellFireToEnemy(pCreature);
                pEffect->setCasterOID(m_CasterID);
                pEffect->setDeadline(m_Duration);
                pEffect->setDamage(m_Damage);
                pEffect->setNextTime(10);
                pCreature->addEffect(pEffect);
                pCreature->setFlag(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);

                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(pCreature->getObjectID());
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_HELLFIRE_TO_ENEMY);
                gcAddEffect.setDuration(m_Duration);

                m_pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
            }
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFire::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFire::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHellFire::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHellFire::toString() const {
    StringStream msg;

    msg << "EffectHellFire(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
