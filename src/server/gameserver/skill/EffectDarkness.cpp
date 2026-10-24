//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDarkness.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDarkness.h"

#include "Creature.h"
#include "DB.h"
#include "Tile.h"
#include "Zone.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

EffectDarkness::EffectDarkness(Zone* pZone, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY)

{
    m_pZone = pZone;
    m_X = ZoneX;
    m_Y = ZoneY;
}

//////////////////////////////////////////////////////////////////////////////
// EffectDarkness::affectCreature()
// When bAffectByMove is false the target is affected in place, so the outer
// SkillOK packet sends the modify info and there is no need to send
// GCModifyInformation here.
// Returns true when pTarget's sight changed.
//////////////////////////////////////////////////////////////////////////////
bool EffectDarkness::affectObject(Object* pTarget, bool bAffectByMove)

{
    bool bAffected = false;
    if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        Creature* pTargetCreature = dynamic_cast<Creature*>(pTarget);
        // The effect is not applied again if it is already on the target.
        if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_DARKNESS)) {
            return false;
        }
        // Only Slayers and Ousters are affected.
        if (pTargetCreature->isSlayer() || pTargetCreature->isOusters()) {
            bAffected = true;
            pTargetCreature->setFlag(Effect::EFFECT_CLASS_DARKNESS);
        }
    }

    return bAffected;
}

void EffectDarkness::unaffectObject(Object* pTarget, bool bUnaffectByMove)

{
    Assert(pTarget != NULL);

    if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
        Creature* pTargetCreature = dynamic_cast<Creature*>(pTarget);

        if (pTargetCreature->isSlayer() || pTargetCreature->isOusters()) {
            pTargetCreature->removeFlag(Effect::EFFECT_CLASS_DARKNESS);
        }
    }
}


//////////////////////////////////////////////////////////////////////////////
// EffectDarkness::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectDarkness::affect(Creature* pTargetCreature)

{}

//////////////////////////////////////////////////////////////////////////////
// EffectDarkness::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectDarkness::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
// unaffect()
//////////////////////////////////////////////////////////////////////////////
void EffectDarkness::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);

    // unaffect creatures on tile
    const forward_list<Object*>& oList = tile.getObjectList();
    for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
        if (*itr != this) {
            EffectDarkness::unaffectObject((Object*)(*itr), false);
        }
    }

    tile.deleteEffect(m_ObjectID);
}

void EffectDarkness::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

string EffectDarkness::toString() const {
    StringStream msg;
    msg << "EffectDarkness(" << "DayTime:" << m_Deadline.tv_sec << ")";
    return msg.toString();
}

void EffectDarknessLoader::load(Zone* pZone)

{
    vector<ZoneEffectBoundsRow> rows =
        defaultZoneInfoRepository().loadZoneEffectBounds(pZone->getZoneID(), Effect::EFFECT_CLASS_DARKNESS);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    for (size_t r = 0; r < rows.size(); r++) {
        int left = rows[r].left;
        int top = rows[r].top;
        int right = rows[r].right;
        int bottom = rows[r].bottom;

        for (int x = left; x <= right; x++) {
            for (int y = top; y <= bottom; y++) {
                if (!rect.ptInRect(x, y))
                    continue;

                Tile& tile = pZone->getTile(x, y);

                if (tile.canAddEffect()) {
                    EffectDarkness* pEffect = new EffectDarkness(pZone, x, y);
                    pEffect->setLevel(300);
                    pEffect->setStartTime();

                    pZone->registerObject(pEffect);
                    tile.addEffect(pEffect);
                }
            }
        }
    }
}
