//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectIceField.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectIceField.h"

#include "DB.h"
#include "EffectIceFieldToCreature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "ZoneUtil.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectIceField::EffectIceField(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    __BEGIN_TRY

    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_CasterName = "";
    m_CasterID = 0;
    m_bForce = false;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceField::affect()

{
    __BEGIN_TRY


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

            // A monster is not hit unless a player cast the skill.
            if (pCreature->isMonster() && isForce()) {
                continue;
            }

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING &&
                !pCreature->isFlag(Effect::EFFECT_CLASS_ICE_FIELD_TO_CREATURE)) {
                // Creates the effect class and attaches it.
                EffectIceFieldToCreature* pEffect = new EffectIceFieldToCreature(pCreature);
                pEffect->setDeadline(m_Duration);
                pCreature->addEffect(pEffect);
                pCreature->setFlag(Effect::EFFECT_CLASS_ICE_FIELD_TO_CREATURE);

                GCAddEffect gcAddEffect;
                gcAddEffect.setObjectID(pCreature->getObjectID());
                gcAddEffect.setEffectID(Effect::EFFECT_CLASS_ICE_FIELD_TO_CREATURE);
                gcAddEffect.setDuration(m_Duration);

                m_pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
            }
        }
    }

    setNextTime(m_Tick);


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceField::affect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceField::unaffect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectIceField::unaffect()

{
    __BEGIN_TRY


    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectIceField::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "EffectIceField(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();

    __END_CATCH
}

void EffectIceFieldLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    vector<ZoneEffectRow> rows =
        defaultZoneInfoRepository().loadZoneEffectRects(pZone->getZoneID(), (int)Effect::EFFECT_CLASS_ICE_FIELD);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value1 = rows[r].value1;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (int X = left; X <= right; X++)
            for (int Y = top; Y <= bottom; Y++) {
                if (rect.ptInRect(X, Y)) {
                    Tile& tile = pZone->getTile(X, Y);
                    if (tile.canAddEffect()) {
                        EffectIceField* pEffect = new EffectIceField(pZone, X, Y);
                        pEffect->setDuration(value1);
                        pEffect->setNextTime(0);
                        pEffect->setTick(10);
                        pEffect->setForce(true);

                        // Register the effect in the zone and add it to the tile.
                        pZone->registerObject(pEffect);
                        tile.addEffect(pEffect);
                    }
                }
            }
    }

    __END_CATCH
}
