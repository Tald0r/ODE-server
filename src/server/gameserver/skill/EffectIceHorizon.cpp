//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectIceHorizon.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectIceHorizon.h"

#include "DB.h"
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
EffectIceHorizon::EffectIceHorizon(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_CasterName = "";
    m_CasterID = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceHorizon::affect()

{
    Assert(m_pZone != NULL);

    // Fetches the creature that used the effect.
    // It may be NULL: the creature may already have left the zone.
    // by bezz. 2003.1.4

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

            if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                continue;
            }

            if (!pCreature->isOusters()) {
                continue;
            }

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                Ousters* pTargetOusters = dynamic_cast<Ousters*>(pCreature);
                Assert(pTargetOusters != NULL);

                GCModifyInformation gcTargetMI;
                GCStatusCurrentHP gcTargetHP;

                HP_t final = min((int)pTargetOusters->getHP(ATTR_MAX), pTargetOusters->getHP() + m_IncreaseAmount);
                if (final > pTargetOusters->getHP(ATTR_MAX) - pTargetOusters->getSilverDamage()) {
                    pTargetOusters->setSilverDamage(pTargetOusters->getHP(ATTR_MAX) - final);
                    gcTargetMI.addShortData(MODIFY_SILVER_DAMAGE, pTargetOusters->getSilverDamage());
                }

                if (pTargetOusters->getHP() != final) {
                    pTargetOusters->setHP(final);
                    gcTargetMI.addShortData(MODIFY_CURRENT_HP, final);
                }

                gcTargetHP.setObjectID(pTargetOusters->getObjectID());
                gcTargetHP.setCurrentHP(final);

                pTargetOusters->getPlayer()->sendPacket(&gcTargetMI);
                m_pZone->broadcastPacket(pTargetOusters->getX(), pTargetOusters->getY(), &gcTargetHP, pTargetOusters);

                GCAddEffect gcAE;
                gcAE.setObjectID(pTargetOusters->getObjectID());
                gcAE.setEffectID(Effect::EFFECT_CLASS_ICE_HORIZON);
                gcAE.setDuration(0);
                m_pZone->broadcastPacket(pTargetOusters->getX(), pTargetOusters->getY(), &gcAE);
            }
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceHorizon::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceHorizon::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceHorizon::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectIceHorizon::toString() const {
    StringStream msg;

    msg << "EffectIceHorizon(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
