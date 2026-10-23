//////////////////////////////////////////////////////////////////////////////
// Filename    : InventorySlot.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "InventorySlot.h"

//////////////////////////////////////////////////////////////////////////////
// class InventorySlot member methods
//////////////////////////////////////////////////////////////////////////////

InventorySlot::~InventorySlot()

{
    try {
        Assert(m_pItem == NULL);
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

void InventorySlot::addItem(Item* pItem)

{
    Assert(pItem != NULL);
    m_pItem = pItem;
}

void InventorySlot::deleteItem()

{
    Assert(m_pItem != NULL);
    m_pItem = NULL;
}
