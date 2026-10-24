//////////////////////////////////////////////////////////////////////////////
// Filename    : UniqueItemManager.cpp
// Written By  : suigui
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "UniqueItemManager.h"

#include "GameContext.h"
#include "Item.h"
#include "ItemInfoManager.h"
#include "repository/ItemRepository.h"


//----------------------------------------------------------------------
// init
//----------------------------------------------------------------------
void UniqueItemManager::init()

{
    __BEGIN_DEBUG

    // Read the current rows and mark each (itemClass, itemType) unique.
    vector<UniqueItemRow> rows = defaultItemRepository().loadUniqueItems();

    for (size_t r = 0; r < rows.size(); r++) {
        Item::ItemClass itemClass = (Item::ItemClass)rows[r].itemClass;
        int itemType = rows[r].itemType;

        ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(itemClass, itemType);
        Assert(pItemInfo != NULL);

        pItemInfo->setUnique();
    }

    __END_DEBUG
}

//----------------------------------------------------------------------
// is Possible Create
//----------------------------------------------------------------------
bool UniqueItemManager::isPossibleCreate(Item::ItemClass itemClass, ItemType_t itemType)

{
    __BEGIN_DEBUG

    // Read the current numbers; an item without a row falls through to the
    // false below, as before.
    int limitNumber = 0;
    int currentNumber = 0;

    if (defaultItemRepository().loadUniqueItemNumbers((int)itemClass, (int)itemType, limitNumber, currentNumber)) {
        return limitNumber == 0 || currentNumber < limitNumber;
    }

    __END_DEBUG

    return false;
}

//----------------------------------------------------------------------
// createItem
//----------------------------------------------------------------------
// Increase the count in the DB
//----------------------------------------------------------------------
void UniqueItemManager::createItem(Item::ItemClass itemClass, ItemType_t itemType)

{
    __BEGIN_DEBUG

    defaultItemRepository().incrementUniqueItemCount((int)itemClass, (int)itemType);

    __END_DEBUG
}

//----------------------------------------------------------------------
// deleteItem
//----------------------------------------------------------------------
// Decrease the count in the DB
//----------------------------------------------------------------------
void UniqueItemManager::deleteItem(Item::ItemClass itemClass, ItemType_t itemType)

{
    __BEGIN_DEBUG

    defaultItemRepository().decrementUniqueItemCount((int)itemClass, (int)itemType);

    __END_DEBUG
}
