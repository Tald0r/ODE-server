//////////////////////////////////////////////////////////////////////////////
// Filename    : Corpse.h
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Corpse.h"

#include <stdio.h>

#include "Inventory.h"
#include "ItemFactory.h"
#include "ItemUtil.h"
#include "RelicUtil.h"
#include "Zone.h"
// #include "EffectHasBloodBible.h"
#include "GCAddEffect.h"

// This can be moved into ItemUtil.h and deleted from here.
extern void saveItemInCorpse(Item* pItem, Corpse* pCorpse);

//////////////////////////////////////////////////////////////////////////////
// class Corpse member methods
//////////////////////////////////////////////////////////////////////////////

Corpse::Corpse()

{
    m_TreasureCount = 0;
    m_X = m_Y = 0;
    m_pZone = NULL;
    m_Level = 0;
    m_Exp = 0;
}

Corpse::~Corpse()

{
    __BEGIN_TRY

    while (!m_Treasures.empty()) {
        Item* pItem = m_Treasures.front();
        SAFE_DELETE(pItem);
        m_Treasures.pop_front();
    }

    __END_CATCH_NO_RETHROW
}

Item* Corpse::getTreasure()

{
    if (!m_Treasures.empty()) {
        Item* pItem = m_Treasures.front();
        Assert(pItem != NULL);

        // Turn the effect off when the bible leaves the shrine.
        deleteFlagEffect(this, pItem);
        deleteRelicEffect(this, pItem);

        m_Treasures.pop_front();
        m_TreasureCount--;

        return pItem;
    }

    return NULL;
}

void Corpse::addTreasure(Item* pItem)

{
    Assert(pItem != NULL);

    if (isRelicItem(pItem)) {
        saveItemInCorpse(pItem, this);

        addHasRelicEffect(m_pZone, this, pItem);
    } else if (pItem->isFlagItem()) {
        saveItemInCorpse(pItem, this);
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
        saveItemInCorpse(pItem, this);
    }

    m_Treasures.push_back(pItem);
    m_TreasureCount++;
}

Item* Corpse::getTreasure(ObjectID_t objectID) const

{
    list<Item*>::const_iterator itr = m_Treasures.begin();

    for (; itr != m_Treasures.end(); itr++) {
        if ((*itr)->getObjectID() == objectID)
            return *itr;
    }

    return NULL;
}


Item* Corpse::popTreasure(ObjectID_t objectID)

{
    list<Item*>::iterator itr = m_Treasures.begin();

    for (; itr != m_Treasures.end(); itr++) {
        if ((*itr)->getObjectID() == objectID) {
            Item* pItem = *itr;
            m_Treasures.erase(itr);
            m_Treasures.push_front(pItem);
            return getTreasure();
        }
    }

    return NULL;
}
