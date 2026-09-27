//////////////////////////////////////////////////////////////////////////////
// Filename    : Item.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Item.h"

#include <cstdio>

#include "Assert.h"
#include "CGRequestStoreInfo.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "PCItemInfo.h"
#include "PlayerCreature.h"
#include "Store.h"
#include "repository/ItemRepository.h"

//////////////////////////////////////////////////////////////////////////////
// class Item member methods
//////////////////////////////////////////////////////////////////////////////

Item::Item()

{
    m_ItemID = 0;
    m_CreateType = CREATE_TYPE_NORMAL;
    m_bUnique = false;
    m_bQuest = false;
    m_bTrace = false;
    m_bTimeLimit = false;
    m_bOnStore = false;
    m_pStore = NULL;
}

Item::~Item()

{}

bool Item::destroy()

{
    __BEGIN_TRY

    // Deleting from the DB; the item may already be gone there.
    if (!defaultItemRepository().deleteItemRow(getObjectTableName(), m_ItemID)) {
        return false;
    }

    return true;

    __END_CATCH
}

// Discard the item. Into the garbage by default, in special cases into Timeover or another unused storage.
void Item::waste(Storage storage) const

{
    Assert((uint)storage >= (uint)STORAGE_GARBAGE);

    char query[50];

    snprintf(query, sizeof(query), "Storage = %u", (uint)storage);

    tinysave(query);
}

const list<OptionType_t>& Item::getDefaultOptions(void) const

{
    __BEGIN_TRY

    try {
        return de::gameContext().itemInfos().getItemInfo(getItemClass(), getItemType())->getDefaultOptions();

    } catch (NoSuchElementException&) {
        StringStream msg;
        msg << "No such item info.(" << (int)getItemClass() << ", " << (int)getItemType() << ")";

        filelog("itemError.txt", "%s", msg.toString().c_str());
        // throw Error(msg.toString());
    }

    static list<OptionType_t> optionTypes;
    return optionTypes;

    __END_CATCH
}

bool Item::isQuestItem() const {
    return (getItemClass() == ITEM_CLASS_EVENT_ITEM && getItemType() < 27) || //(!= 27 && getItemType() != 28) ||
           (getItemClass() == ITEM_CLASS_EVENT_TREE && getItemType() > 12 && getItemType() < 26) ||
           (getItemClass() == ITEM_CLASS_EVENT_TREE && getItemType() > 28 && getItemType() < 42) ||
           (getItemClass() == ITEM_CLASS_CODE_SHEET);
}

void Item::makePCItemInfo(PCItemInfo& result) const {
    if (m_ObjectID == 0) {
        filelog("ItemError.log", "Item oid is 0. : %s", toString().c_str());
        result.setObjectID(0);
    } else {
        result.setObjectID(getObjectID());
    }

    result.setItemClass(getItemClass());
    result.setItemType(getItemType());
    result.setOptionType(getOptionTypeList());
    result.setSilver(getSilver());
    result.setDurability(getDurability());
    result.setEnchantLevel(getEnchantLevel());
    result.setItemNum(getNum());
    result.setMainColor(0);
    result.setGrade(getGrade());
}

void Item::whenPCTake(PlayerCreature* pPC) {
    //	cout << pPC->getName() << " Take " << toString() << endl;
}

void Item::whenPCLost(PlayerCreature* pPC) {
    //	cout << pPC->getName() << " Lost " << toString() << endl;
    Store* pStore = pPC->getStore();
    BYTE index = pStore->getItemIndex(this);

    if (index != 0xff) {
        pStore->removeStoreItem(index);

        CGRequestStoreInfo cgInfo;
        cgInfo.setOwnerObjectID(0);
        // synthetic packet straight to the handler (2.3)
        CGRequestStoreInfoHandler::execute(&cgInfo, pPC->getPlayer());
    }
}
