//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSanctuary.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSanctuary.h"

#include "Creature.h"
#include "Tile.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

EffectSanctuary::EffectSanctuary(Zone* pZone, ZoneCoord_t ZoneX, ZoneCoord_t ZoneY, ZoneCoord_t CenterX,
                                 ZoneCoord_t CenterY)

{
    m_pZone = pZone;
    m_X = ZoneX;
    m_Y = ZoneY;

    m_CenterX = CenterX;
    m_CenterY = CenterY;
}

//////////////////////////////////////////////////////////////////////////////
// EffectSanctuary::affectCreature()
// When bAffectByMove is false the target is affected in place, so the outer
// SkillOK packet sends the modify info and there is no need to send
// GCModifyInformation here.
// Returns true when pTarget's sight changed.
//////////////////////////////////////////////////////////////////////////////
bool EffectSanctuary::affectObject(Object* pTarget, bool bAffectByMove)

{
    return false;
}

void EffectSanctuary::unaffectObject(Object* pTarget, bool bUnaffectByMove)

{}


//////////////////////////////////////////////////////////////////////////////
// EffectSanctuary::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectSanctuary::affect(Creature* pTargetCreature)

{}

//////////////////////////////////////////////////////////////////////////////
// EffectSanctuary::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectSanctuary::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
// unaffect()
//////////////////////////////////////////////////////////////////////////////
void EffectSanctuary::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);

    tile.deleteEffect(m_ObjectID);
}

void EffectSanctuary::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

string EffectSanctuary::toString() const {
    StringStream msg;
    msg << "EffectSanctuary(" << "DayTime:" << m_Deadline.tv_sec << ")";
    return msg.toString();
}
