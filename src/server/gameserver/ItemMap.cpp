////////////////////////////////////////////////////////////////////////////////
// Filename : ItemMap.h
// Description :
// A sorted map of items taken from the client.
////////////////////////////////////////////////////////////////////////////////

#include "ItemMap.h"

#include "Inventory.h"
#include "Item.h"

#define TWO_BY_TWO_PACKING_SIZE 12

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
ItemMap::ItemMap()

{
    m_Num2x2 = 0;
    m_Num2x2Temp = 0;
}

////////////////////////////////////////////////////////////
// If the item objects themselves should not be deleted,
// be sure to call clearAll() before this.
////////////////////////////////////////////////////////////
ItemMap::~ItemMap()

{
    clear();
}

////////////////////////////////////////////////////////////
// Set the number of 2x2 items, used for improved sorting.
////////////////////////////////////////////////////////////
void ItemMap::set2x2(int n)

{
    // When there are three 2x2 items they have the
    // highest priority, so the variables are set in
    // units of three.
    m_Num2x2 = n;
    m_Num2x2Temp = (n / 3) * 3;
}

////////////////////////////////////////////////////////////
// Add an item to the map.
////////////////////////////////////////////////////////////
bool ItemMap::addItem(Item* pItem)

{
    ulonglong key = getKey(pItem);

    ITEM_MAP::const_iterator itr = find(key);

    if (itr == end()) // Add it if it is not there.
    {
        insert(ITEM_MAP::value_type(key, pItem));
        return true;
    }

    return false;
}


////////////////////////////////////////////////////////////
// Build the key used to sort items.
//
// The key is 8 bytes, starting from the high byte..
//
// 4 Byte : gridWidth* gridHeight
// 4 Byte : ObjectID
//
// is how it is laid out.
////////////////////////////////////////////////////////////
ulonglong ItemMap::getKey(Item* pItem)

{
    ObjectID_t objectID = pItem->getObjectID();
    int gridWidth = pItem->getVolumeWidth();
    int gridHeight = pItem->getVolumeHeight();
    int gridSize = gridWidth * gridHeight;

    // As a stopgap, 2x2 items are given the highest priority.
    if (gridSize == 4) {
        if (m_Num2x2Temp > 0) {
            m_Num2x2Temp--;
            gridSize = TWO_BY_TWO_PACKING_SIZE;
        }
    }

    gridSize = 0xFF - gridSize;

    ulonglong key = gridSize;
    key = (key << 32) | objectID;

    return key;
}
