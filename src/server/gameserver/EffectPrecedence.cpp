//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectPrecedence.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectPrecedence.h"

#include "Assert.h"
#include "Creature.h"
#include "DB.h"
#include "Zone.h"

EffectPrecedence::EffectPrecedence(Creature* pCreature)

{
    Assert(pCreature != NULL);
    Assert(pCreature->isMonster());

    setTarget(pCreature);

    // Server-only Effect.
    m_bBroadcastingEffect = false;

    m_bItem = false;
}

EffectPrecedence::EffectPrecedence(Item* pItem)

{
    Assert(pItem != NULL);
    setTarget(pItem);

    m_bItem = true;

    // Server-only Effect.
    m_bBroadcastingEffect = false;
}

void EffectPrecedence::affect()

{}

void EffectPrecedence::affect(Creature* pCreature)

{}

void EffectPrecedence::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    throw UnsupportedError();
}

void EffectPrecedence::unaffect()

{
    if (m_bItem) {
        Item* pItem = dynamic_cast<Item*>(m_pTarget);
        pItem->removeFlag(getEffectClass());
    } else {
        Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
        Assert(pCreature != NULL);
        Assert(pCreature->isMonster());
        pCreature->removeFlag(getEffectClass());
    }
}

void EffectPrecedence::unaffect(Creature* pCreature)

{
    Assert(false);
}

void EffectPrecedence::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pTarget)

{
    Assert(false);
}

string EffectPrecedence::toString() const

{
    StringStream msg;
    msg << "EffectPrecedence(" << "HostName:" << m_HostName << ",PartyID:" << m_HostPartyID << ",bItem:" << m_bItem
        << ")";
    return msg.toString();
}
