//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRelicTable.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectRelicTable.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "Item.h"
#include "Monster.h"
#include "Player.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRelicTable::EffectRelicTable(Item* pItem)

{
    m_SafeTime.tv_sec = 0;
    m_SafeTime.tv_usec = 0;

    m_LockTime.tv_sec = 0;
    m_LockTime.tv_usec = 0;

    setTarget(pItem);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicTable::affect()

{
    Item* pItem = dynamic_cast<Item*>(m_pTarget);
    affect(pItem);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicTable::affect(Item* pItem)

{}

void EffectRelicTable::unaffect()

{
    Item* pItem = dynamic_cast<Item*>(m_pTarget);
    unaffect(pItem);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRelicTable::unaffect(Item* pItem)

{
    __BEGIN_DEBUG

    pItem->removeFlag(getEffectClass());

    __END_DEBUG
}

// The relic cannot be taken out until SafeTime has passed.
bool EffectRelicTable::isSafeTime() const {
    Timeval currentTime;
    getCurrentTime(currentTime);

    return currentTime > m_SafeTime;
}

// The relic cannot be taken out during LockTime.
bool EffectRelicTable::isLockTime() const {
    Timeval currentTime;
    getCurrentTime(currentTime);

    return currentTime < m_LockTime;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectRelicTable::toString() const {
    StringStream msg;
    msg << "EffectRelicTable(" << "SafeTime:" << m_SafeTime.tv_sec << ")";
    return msg.toString();
}
