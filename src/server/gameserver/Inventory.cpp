////////////////////////////////////////////////////////////////////////////////
// Filename    : Inventory.cpp
// Written By  : elca@ewestsoft.com
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "Inventory.h"

#include <unordered_map>

#include "Assert.h"
#include "Belt.h"
#include "EffectSchedule.h"
#include "EffectVampirePortal.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Key.h"
#include "ObjectRegistry.h"
#include "ParkingCenter.h"
#include "Store.h"
#include "VolumeInfo.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
//
// CONSTRUCTOR & DESTRUCTOR
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Default constructor.
////////////////////////////////////////////////////////////
Inventory::Inventory(CoordInven_t Width, CoordInven_t Height, bool bDeleteAll)

{
    m_Owner = "";
    m_Width = Width;
    m_Height = Height;
    m_TotalNum = 0;
    m_TotalWeight = 0;
    m_bDeleteAll = bDeleteAll;
    m_pInventorySlot = NULL;

    m_pInventorySlot = new InventorySlot*[m_Width];
    for (int i = 0; i < m_Width; i++) {
        m_pInventorySlot[i] = new InventorySlot[m_Height];
    }
}

////////////////////////////////////////////////////////////
// Copy constructor.
////////////////////////////////////////////////////////////
Inventory::Inventory(const Inventory* pInventory)

{
    m_Width = pInventory->getWidth();
    m_Height = pInventory->getHeight();
    m_TotalNum = pInventory->getItemNum();
    m_TotalWeight = pInventory->getWeight();
    m_bDeleteAll = pInventory->getDeleteAllFlag();
    m_pInventorySlot = NULL;

    m_pInventorySlot = new InventorySlot*[m_Width];
    for (int i = 0; i < m_Width; i++) {
        m_pInventorySlot[i] = new InventorySlot[m_Height];
    }

    for (int x = 0; x < m_Width; x++) {
        for (int y = 0; y < m_Height; y++) {
            Item* pItem = pInventory->getInventorySlot(x, y).getItem();
            if (pItem != NULL)
                m_pInventorySlot[x][y].addItem(pItem);
        }
    }
}

////////////////////////////////////////////////////////////
// Destructor.
////////////////////////////////////////////////////////////
Inventory::~Inventory()

{
    int i, j, k;

    try {
        if (m_pInventorySlot != NULL) {
            // Delete every item held in the inventory.
            // Using the item size would allow further optimization.
            for (j = 0; j < m_Height; j++) {
                for (i = 0; i < m_Width; i++) {
                    Item* pItem = m_pInventorySlot[i][j].getItem();
                    if (pItem != NULL) {
                        // Set the inventory slot to NULL.

                        deleteItem(i, j);

                        if (m_bDeleteAll) {
                            // For a key, take the motorcycle information out of the ParkingCenter,
                            // remove the motorcycle from the Zone, then delete it from the ParkingCenter.
                            // Cleanly this would belong in the destructor of GamePlayer or Creature,
                            // but it is done here as a shortcut to save search time.
                            // Removing this breaks things.
                            if (pItem->getItemClass() == Item::ITEM_CLASS_KEY) {
                                Key* pKey = dynamic_cast<Key*>(pItem);
                                // The call below also takes care of removing it from the zone.
                                if (de::gameContext().parking().hasMotorcycleBox(pKey->getTarget())) {
                                    de::gameContext().parking().deleteMotorcycleBox(pKey->getTarget());
                                }
                            }

                            SAFE_DELETE(pItem);
                        } // end of if (m_bDeleteAll)
                    } // end of if (pItem != NULL)
                } // end of for
            } // end of for

            // Assert(m_TotalNum == 0);
            // Assert(m_TotalWeight == 0);
            // Temporary code, just to look at the number.
            if (m_TotalNum != 0) {
                filelog("inventoryBug.txt", "TotalNum=%d", m_TotalNum);
            }

            Assert(m_TotalNum == 0);

            for (k = 0; k < m_Width; k++) {
                if (m_pInventorySlot[k] != NULL) {
                    SAFE_DELETE_ARRAY(m_pInventorySlot[k]);
                }
            }

            SAFE_DELETE_ARRAY(m_pInventorySlot);
        }
    } catch (Throwable& t) {
        // cerr << t.toString() << endl;
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// CHECK METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Is there an item at the given position?
////////////////////////////////////////////////////////////
bool Inventory::hasItem(CoordInven_t X, CoordInven_t Y)

{
    if (X < m_Width && Y < m_Height) {
        InventorySlot& slot = getInventorySlot(X, Y);
        return (slot.getItem() != NULL) ? true : false;
    }
    return false;
}

////////////////////////////////////////////////////////////
// Is the given item present?
////////////////////////////////////////////////////////////
bool Inventory::hasItem(ObjectID_t ObjectID)

{
    CoordInven_t x, y;
    if (findItemOID(ObjectID, x, y) != NULL)
        return true;
    return false;
}

////////////////////////////////////////////////////////////
// Is the given item present?
////////////////////////////////////////////////////////////
bool Inventory::hasItemWithItemID(ItemID_t ItemID)

{
    CoordInven_t x, y;
    if (findItemIID(ItemID, x, y) != NULL)
        return true;
    return false;
}

////////////////////////////////////////////////////////////
// Is a key aimed at the given target held?
////////////////////////////////////////////////////////////
bool Inventory::hasKey(ItemID_t TargetItemID)

{
    for (int x = 0; x < m_Width; x++) {
        for (int y = 0; y < m_Height; y++) {
            Item* pSlotItem = m_pInventorySlot[x][y].getItem();

            if (pSlotItem != NULL && pSlotItem->getItemClass() == Item::ITEM_CLASS_KEY) {
                Key* pKey = dynamic_cast<Key*>(pSlotItem);
                if (pKey->getTarget() == TargetItemID)
                    return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////
// Can the item be added at the given position?
// This function assumes that one item can be attached to the mouse
// and returns the result accordingly.
////////////////////////////////////////////////////////////
bool Inventory::canAdding(CoordInven_t X, CoordInven_t Y, Item* pItem)

{
    // get Volume's Size Width, Height
    VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
    VolumeHeight_t ItemHeight = pItem->getVolumeHeight();
    int ItemCount = 0;
    ObjectID_t ItemObjectID;

    if ((X + ItemWidth > m_Width) || (Y + ItemHeight > m_Height))
        return false;

    for (int x = X; x < X + ItemWidth; x++) {
        for (int y = Y; y < Y + ItemHeight; y++) {
            if (hasItem(x, y)) {
                Item* pTempItem = m_pInventorySlot[x][y].getItem();
                ObjectID_t TempObjectID = pTempItem->getObjectID();

                if (ItemCount == 0) {
                    ItemObjectID = TempObjectID;
                    ItemCount++;
                }
                if (ItemObjectID != TempObjectID) {
                    ItemCount++;
                }
            }
        }
    }

    if (ItemCount > 1)
        return false;

    // if not false return true
    return true;
}

////////////////////////////////////////////////////////////
// Can the item be placed at the given position?
// This function does ***not*** assume that an item can be
// attached to the mouse.
////////////////////////////////////////////////////////////
bool Inventory::canAddingEx(CoordInven_t X, CoordInven_t Y, Item* pItem)

{
    VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
    VolumeHeight_t ItemHeight = pItem->getVolumeHeight();
    list<Item*> prevItemList;

    if ((X + ItemWidth > m_Width) || (Y + ItemHeight > m_Height))
        return false;

    // Scan the inventory; where a slot holds an item, check that the
    // item is not already in the list, then put it into the list.
    for (int x = X; x < X + ItemWidth; x++) {
        for (int y = Y; y < Y + ItemHeight; y++) {
            Item* pInvenItem = m_pInventorySlot[x][y].getItem();

            // If there is an item in that slot...
            if (pInvenItem != NULL) {
                bool bAdd = true;

                // Check whether it is already in the list.
                list<Item*>::iterator itr = prevItemList.begin();
                for (; itr != prevItemList.end(); itr++) {
                    if (*itr == pInvenItem) {
                        bAdd = false;
                        break;
                    }
                }

                // Add the item to the list.
                if (bAdd)
                    prevItemList.push_back(pInvenItem);
            }
        }
    }

    // If the target area already holds two or more different items,
    // the item cannot be placed.
    if (prevItemList.size() > 1)
        return false;

    // If exactly one item is there, it must be a stackable item and
    // must have the same class and type as the item being stacked.
    if (prevItemList.size() == 1) {
        Item::ItemClass IClass = pItem->getItemClass();
        ItemType_t IType = pItem->getItemType();

        Item* pInvenItem = prevItemList.front();
        Item::ItemClass InvenIClass = pInvenItem->getItemClass();
        ItemType_t InvenIType = pInvenItem->getItemType();

        // A different kind of item means false.
        if (IClass != InvenIClass || IType != InvenIType)
            return false;

        // An item that cannot be stacked means false.
        if (!isStackable(pItem))
            return false;

        // Exceeding the maximum count also means false.
        uint MaxStack = ItemMaxStack[IClass];
        if ((pItem->getNum() + pInvenItem->getNum()) > (int)(MaxStack))
            return false;
    }

    prevItemList.clear();

    return true;
}


////////////////////////////////////////////////////////////////////////////////
//
// ADDITION & DELETION RELATED METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Add an item at the given position.
////////////////////////////////////////////////////////////
bool Inventory::addItem(CoordInven_t X, CoordInven_t Y, Item* pItem)

{
    __BEGIN_DEBUG

    if (pItem == NULL) {
        // cerr << "Inventory::addItem() : the item pointer is null." << endl;
        return false;
    }

    VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
    VolumeHeight_t ItemHeight = pItem->getVolumeHeight();
    Weight_t ItemWeight = pItem->getWeight();

    // Verify before adding the item.
    for (int x = X; x < X + ItemWidth; x++)
        for (int y = Y; y < Y + ItemHeight; y++)
            if (getInventorySlot(x, y).getItem() != NULL)
                return false;

    // Add Item to Inventory
    for (int x = X; x < X + ItemWidth; x++) {
        for (int y = Y; y < Y + ItemHeight; y++) {
            InventorySlot& slot = getInventorySlot(x, y);
            slot.addItem(pItem);
        }
    }

    // Add the weight for the item count, and add the count too.
    m_TotalWeight += (ItemWeight * pItem->getNum());
    m_TotalNum += pItem->getNum();

    return true;

    __END_DEBUG
}

////////////////////////////////////////////////////////////
// Add an item at the given position.
////////////////////////////////////////////////////////////
Item* Inventory::addItemEx(CoordInven_t X, CoordInven_t Y, Item* pItem)

{
    __BEGIN_DEBUG

    if (!canAddingEx(X, Y, pItem)) {
        // cerr << "Inventory::addItemEx() : canAddingExCheck failed!!!" << endl;
        // cerr << toString() << endl;
        throw Error("Inventory::addItemEx() : cannot add the item!");
    }

    VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
    VolumeHeight_t ItemHeight = pItem->getVolumeHeight();
    Item* pInvenItem = m_pInventorySlot[X][Y].getItem();

    // If an item is already there it is a stackable item,
    // so increase its count.
    if (pInvenItem != NULL) {
        pInvenItem->setNum(pItem->getNum() + pInvenItem->getNum());

        // Before deleting the pointer, increase the weight and the item count.
        m_TotalWeight += (pItem->getWeight() * pItem->getNum());
        m_TotalNum += pItem->getNum();

        // The inventory item's count has been increased, so the item
        // passed in to be added is deleted.
        // *** The item was deleted here originally, but that seemed
        // wrong, so for now it is left alone. ***
        // SAFE_DELETE(pItem);
        // pItem = NULL;
        return pInvenItem;
    }

    for (int x = X; x < X + ItemWidth; x++) {
        for (int y = Y; y < Y + ItemHeight; y++) {
            // If no item is there, assign the same pointer to every slot.
            m_pInventorySlot[x][y].addItem(pItem);
        }
    }

    m_TotalWeight += (pItem->getWeight() * pItem->getNum());
    m_TotalNum += pItem->getNum();

    return pItem;

    __END_DEBUG
}

////////////////////////////////////////////////////////////
// Add an item at an automatically chosen position.
////////////////////////////////////////////////////////////
bool Inventory::addItem(Item* pItem) {
    Assert(pItem != NULL);

    _TPOINT pt;

    if (getEmptySlot(pItem, pt)) {
        addItem(pt.x, pt.y, pItem);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////
// Add an item at an automatically chosen position.
////////////////////////////////////////////////////////////
bool Inventory::addItem(Item* pItem, TPOINT& rpt) {
    Assert(pItem != NULL);

    _TPOINT pt;

    if (getEmptySlot(pItem, pt)) {
        rpt.x = pt.x;
        rpt.y = pt.y;
        addItem(pt.x, pt.y, pItem);
        return true;
    }

    rpt.x = 255;
    rpt.y = 255;

    return false;
}

////////////////////////////////////////////////////////////
// Find an empty place where the item can be put.
////////////////////////////////////////////////////////////
bool Inventory::getEmptySlot(VolumeWidth_t ItemWidth, VolumeHeight_t ItemHeight, _TPOINT& p)

{
    int x, y;
    int i, j;

    //---------------------------------------------------------
    // Check whether pItem can be added at every (x,y) of the grid.
    //---------------------------------------------------------
    int yLimit = m_Height - ItemHeight;
    int xLimit = m_Width - ItemWidth;

    int yPlusHeight, xPlusWidth;

    for (x = 0; x <= xLimit; x++) {
        xPlusWidth = x + ItemWidth;

        for (y = 0; y <= yLimit; y++) {
            yPlusHeight = y + ItemHeight;

            //---------------------------------------------------------
            // Check whether it can be put at (x,y).
            //---------------------------------------------------------
            bool bPlace = true;

            for (i = y; bPlace && i < yPlusHeight; i++) {
                for (j = x; bPlace && j < xPlusWidth; j++) {
                    //---------------------------------------------------------
                    // If even one grid cell already holds another Item, it cannot be added.
                    //---------------------------------------------------------
                    Item* pItem = m_pInventorySlot[j][i].getItem();
                    if (pItem != NULL) {
                        bPlace = false;

                        // Next thing to check...
                        //		y = i + pItem->getVolumeHeight() - 1;

                        break;
                    }
                }
            }

            //---------------------------------------------------------
            // The case where it can be put at (x,y).
            //---------------------------------------------------------
            if (bPlace) {
                p.x = x;
                p.y = y;

                // cout << (int)x << ", " << (int)y << " ]" << endl;
                return true;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////
// Find the given item and delete it.
////////////////////////////////////////////////////////////
void Inventory::deleteItem(ObjectID_t ObjectID)

{
    __BEGIN_DEBUG

    Item* pTargetItem = NULL;
    CoordInven_t x = 0;
    CoordInven_t y = 0;

    pTargetItem = findItemOID(ObjectID, x, y);

    if (pTargetItem != NULL) {
        deleteItem(x, y);
    } else
        Assert(false);

    __END_DEBUG
}

////////////////////////////////////////////////////////////
// Delete the item at the given position.
// *** Caution ***
// For an item that is not 1x1, failing to give the top-left
// coordinate can have terrible results.
////////////////////////////////////////////////////////////
void Inventory::deleteItem(CoordInven_t X, CoordInven_t Y)

{
    __BEGIN_DEBUG

    if (X < m_Width && Y < m_Height) {
        Item* pItem = m_pInventorySlot[X][Y].getItem();
        // Assert(pItem != NULL);

        if (pItem != NULL) {
            // get Volume's Size Width, Height
            VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
            VolumeHeight_t ItemHeight = pItem->getVolumeHeight();

            if (pItem->isOnStore()) {
                Store* pStore = pItem->getStore();
                if (pStore != NULL && pStore->hasItem(pItem)) {
                    BYTE index = pStore->getItemIndex(pItem);
                    pStore->removeStoreItem(index);
                }
            }

            Assert(ItemWidth != 0);
            Assert(ItemHeight != 0);

            for (int x = X; x < X + ItemWidth; x++) {
                for (int y = Y; y < Y + ItemHeight; y++) {
                    InventorySlot& slot = getInventorySlot(x, y);

                    // if (slot.getItem()==pItem)
                    { slot.deleteItem(); }
                }
            }

            m_TotalWeight -= (pItem->getWeight() * pItem->getNum());
            m_TotalNum -= pItem->getNum();
        } else {
        }
    }

    __END_DEBUG
}


////////////////////////////////////////////////////////////////////////////////
//
// FIND METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Inspect the slots covering the item's size starting at X, Y and
// return the pointer of an item found there.
////////////////////////////////////////////////////////////
Item* Inventory::searchItem(CoordInven_t X, CoordInven_t Y, Item* pItem, TPOINT& pt)

{
    // get Volume's Size Width, Height
    VolumeWidth_t ItemWidth = pItem->getVolumeWidth();
    VolumeHeight_t ItemHeight = pItem->getVolumeHeight();

    if ((X + ItemWidth <= m_Width) && (Y + ItemHeight <= m_Height)) {
        for (int x = X; x < (X + ItemWidth); x++) {
            for (int y = Y; y < (Y + ItemHeight); y++) {
                if (hasItem(x, y)) {
                    pt.x = x;
                    pt.y = y;
                    return m_pInventorySlot[x][y].getItem();
                }
            }
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////
// Find the given item and return its pointer.
////////////////////////////////////////////////////////////
Item* Inventory::getItemWithItemID(ItemID_t itemID)

{
    CoordInven_t x, y;
    return findItemIID(itemID, x, y);
}

////////////////////////////////////////////////////////////
// Find the given belt and return its pointer.
////////////////////////////////////////////////////////////
Item* Inventory::getBeltWithItemID(ItemID_t itemID)

{
    CoordInven_t x, y;
    return findItemIID(itemID, Item::ITEM_CLASS_BELT, x, y);
}

////////////////////////////////////////////////////////////
// Find the given item and return its pointer.
////////////////////////////////////////////////////////////
Item* Inventory::getItemWithObjectID(ObjectID_t objectID)

{
    CoordInven_t x, y;
    return findItemOID(objectID, x, y);
}

////////////////////////////////////////////////////////////
// Find the given item and return its pointer.
// The item's top-left coordinate is returned along with it.
////////////////////////////////////////////////////////////
Item* Inventory::findItemOID(ObjectID_t id, CoordInven_t& X, CoordInven_t& Y)

{
    for (int j = 0; j < m_Height; j++) {
        for (int i = 0; i < m_Width; i++) {
            InventorySlot& slot = getInventorySlot(i, j);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getObjectID() == id) {
                X = i;
                Y = j;
                return pItem;
            }
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////
// Find the given item and return its pointer.
// The item's top-left coordinate is returned along with it.
////////////////////////////////////////////////////////////
Item* Inventory::findItemIID(ItemID_t id, CoordInven_t& X, CoordInven_t& Y)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemID() == id) {
                X = x;
                Y = y;
                return pItem;
            }
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////
// Find the item by the given id and class and return its pointer.
// The item's top-left coordinate is returned along with it.
////////////////////////////////////////////////////////////
Item* Inventory::findItemOID(ObjectID_t id, Item::ItemClass IClass, CoordInven_t& X, CoordInven_t& Y)

{
    for (int j = 0; j < m_Height; j++) {
        for (int i = 0; i < m_Width; i++) {
            InventorySlot& slot = getInventorySlot(i, j);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == IClass && pItem->getObjectID() == id) {
                X = i;
                Y = j;
                return pItem;
            }
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////
// Find the item by the given id and class and return its pointer.
// The item's top-left coordinate is returned along with it.
////////////////////////////////////////////////////////////
Item* Inventory::findItemIID(ItemID_t id, Item::ItemClass IClass, CoordInven_t& X, CoordInven_t& Y)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == IClass && pItem->getItemID() == id) {
                X = x;
                Y = y;
                return pItem;
            }
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// findItem
//    : ItemClass
//  Description: Check whether an item of a given Item Class exists in this inventory.
//               Returns the item of that Item Class the player holds, so the item
//               dropped when a lair master is killed can be exchanged.
//
/////////////////////////////////////////////////////////////////////////////////////

Item* Inventory::findItem(Item::ItemClass IClass, ItemType_t itemType) //, CoordInven_t& X, CoordInven_t& Y)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == IClass &&
                (itemType == 0xFFFF || pItem->getItemType() == itemType)) {
                // X = x;
                // Y = y;
                return pItem;
            }
        }
    }

    return NULL;
}

Item* Inventory::findItem(Item::ItemClass IClass, ItemType_t itemType, CoordInven_t& X, CoordInven_t& Y)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == IClass &&
                (itemType == 0xFFFF || pItem->getItemType() == itemType)) {
                X = x;
                Y = y;
                return pItem;
            }
        }
    }

    //	X = -1;
    //	Y = -1;
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
//
// ITEM MANIPULATION RELATED METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Return the item pointer at the given position.
////////////////////////////////////////////////////////////
Item* Inventory::getItem(CoordInven_t X, CoordInven_t Y) const

{
    InventorySlot& slot = getInventorySlot(X, Y);
    return slot.getItem();
}

////////////////////////////////////////////////////////////
// Set the item pointer at the given position.
////////////////////////////////////////////////////////////
void Inventory::setItem(CoordInven_t X, CoordInven_t Y, Item* pItem)

{
    InventorySlot& slot = getInventorySlot(X, Y);
    slot.addItem(pItem);
}

////////////////////////////////////////////////////////////////////////////////
//
// PACKING RELATED METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Remove every item pointer from the inventory.
// The item objects themselves are not deleted.
////////////////////////////////////////////////////////////
void Inventory::clear()

{
    // Scan right through the whole inventory...
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            slot.deleteItem();
        }
    }

    m_TotalNum = 0;
    m_TotalWeight = 0;
}

////////////////////////////////////////////////////////////
// Build a list of the items held in the inventory and return it.
////////////////////////////////////////////////////////////
list<Item*> Inventory::getList() const

{
    list<Item*> itemList;

    for (int x = 0; x < m_Width; x++) {
        for (int y = 0; y < m_Height; y++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();
            bool bAdd = true;

            // If there is an item, check that it is not already
            // in the item list.
            if (pItem != NULL) {
                // Check whether the same one is already in the list.
                list<Item*>::iterator itr = itemList.begin();
                for (; itr != itemList.end(); itr++) {
                    if (*itr == pItem) {
                        bAdd = false;
                        break;
                    }
                }

                // If the same item was not in the list, add it to the list.
                if (bAdd) {
                    itemList.push_back(pItem);
                    y += pItem->getVolumeHeight() - 1;
                    continue;
                }
            }
        }
    }

    return itemList;
}

////////////////////////////////////////////////////////////
// Return the number of 2x2 items held in the inventory.
////////////////////////////////////////////////////////////
int Inventory::calc2x2Item(void) const

{
    int rValue = 0;
    list<Item*> itemList = getList();

    list<Item*>::const_iterator itr = itemList.begin();
    for (; itr != itemList.end(); itr++) {
        Item* pItem = (*itr);
        if (pItem->getVolumeWidth() == 2 && pItem->getVolumeHeight() == 2)
            rValue += 1;
    }

    return rValue;
}


////////////////////////////////////////////////////////////////////////////////
//
// MISC METHODS
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
void Inventory::save(const string& owner)

{
    list<Item*> itemList;

    for (int x = 0; x < m_Width; x++) {
        for (int y = 0; y < m_Height; y++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();
            bool bAdd = true;

            // If there is an item, check that it is not already
            // in the item list.
            if (pItem != NULL) {
                // Check whether the same one is already in the list.
                list<Item*>::iterator itr = itemList.begin();
                for (; itr != itemList.end(); itr++) {
                    if (*itr == pItem) {
                        bAdd = false;
                        break;
                    }
                }

                // If the same item was not in the list, add it to the list.
                // For a belt, Belt::save also saves the items inside it,
                // so there is nothing to worry about.
                if (bAdd) {
                    pItem->save(owner, STORAGE_INVENTORY, 0, x, y);
                    itemList.push_back(pItem);
                    // y += pItem->getVolumeHeight() - 1;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
string Inventory::toString() const

{
    StringStream msg;
    msg << "Inventory(" << "\n";
    msg << "Owner:" << m_Owner << "\n";

    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL) {
                msg << pItem->getNum();
            }
        }

        msg << "\n";
    }

    msg << "\n";
    msg << ")";

    return msg.toString();
}


///////////////////////////////////////////////////////////////////////////
// Per-colour event star count code, written for the XMAS event.
// The comments were removed so that this code could be reused
// unchanged for the Children's Day event.
// Since the same event can keep recurring, renaming the EVENT
// CODE from XMAS to STAR_EVENT_CODE should be
// considered.
//
//
//////////////////////////////////////////////////////////////////////////
// #ifdef __XMAS_EVENT_CODE__
// Scan the inventory and count the event stars by colour.
bool Inventory::hasEnoughStar(const XMAS_STAR& star)

{
    // cout << "Number of stars needed : " << star.amount << endl;

    int amount[STAR_COLOR_MAX];
    memset(amount, 0, sizeof(int) * STAR_COLOR_MAX);

    for (int i = 0; i < STAR_COLOR_MAX; i++)
        amount[i] = 0;

    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_STAR) {
                int ItemNum = pItem->getNum();
                // cout << pItem->getItemType() << " " << ItemNum << endl;
                switch (pItem->getItemType()) {
                case 0:
                    amount[STAR_COLOR_BLACK] += ItemNum;
                    break;
                case 1:
                    amount[STAR_COLOR_RED] += ItemNum;
                    break;
                case 2:
                    amount[STAR_COLOR_BLUE] += ItemNum;
                    break;
                case 3:
                    amount[STAR_COLOR_GREEN] += ItemNum;
                    break;
                case 4:
                    amount[STAR_COLOR_CYAN] += ItemNum;
                    break;
                case 5:
                    amount[STAR_COLOR_WHITE] += ItemNum;
                    break;
                case 6:
                    amount[STAR_COLOR_PINK] += ItemNum;
                    break;
                default:
                    Assert(false);
                    break;
                }
            }
        }
    }

    // cout << star.color << endl;
    // cout << "Number of balls held: " << amount[star.color] << endl;

    if (amount[star.color] >= star.amount)
        return true;

    return false;
}
// #endif

// #ifdef __XMAS_EVENT_CODE__
void Inventory::decreaseStar(const XMAS_STAR& star)

{
    // Remember the amount that has to be reduced.
    int amount = star.amount;

    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_STAR) {
                ItemType_t IType = pItem->getItemType();
                cout << IType << " " << star.color << endl;

                if ((IType == 0 && star.color == STAR_COLOR_BLACK) || (IType == 1 && star.color == STAR_COLOR_RED) ||
                    (IType == 2 && star.color == STAR_COLOR_BLUE) || (IType == 3 && star.color == STAR_COLOR_GREEN) ||
                    (IType == 4 && star.color == STAR_COLOR_CYAN) || (IType == 5 && star.color == STAR_COLOR_WHITE) ||
                    (IType == 6 && star.color == STAR_COLOR_PINK)) {
                    int ItemNum = pItem->getNum();

                    // If the item's stack count is less than or equal to the amount to reduce,
                    // the item has to be deleted.
                    if (ItemNum <= amount) {
                        m_TotalWeight -= (pItem->getWeight() * ItemNum);
                        m_TotalNum -= ItemNum;

                        // Reduce the remaining amount by however much was deleted.
                        amount = amount - ItemNum;

                        // Delete the item.
                        deleteItem(x, y);
                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    } else {
                        m_TotalWeight -= (pItem->getWeight() * amount);
                        m_TotalNum -= amount;

                        pItem->setNum(ItemNum - amount);
                        pItem->save(m_Owner, STORAGE_INVENTORY, 0, x, y);

                        // Reduce the remaining amount by however much was deleted.
                        amount = 0;
                    }

                    // Return once the amount to reduce reaches 0.
                    if (amount == 0)
                        return;
                }
            }
        }
    }

    // Normal processing should never reach this point.
    Assert(false);
}
// #endif


///*
// #ifdef __XMAS_EVENT_CODE__
bool Inventory::hasRedGiftBox(void)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX &&
                pItem->getItemType() == 1) {
                return true;

                // A present box is 2x2, so
                // advance x by one more.
                x += 1;
            }
        }
    }

    return false;
}
// #endif
//*/

///*
// #ifdef __XMAS_EVENT_CODE__
bool Inventory::hasGreenGiftBox(void)

{
    for (int y = 0; y < m_Height; y++) {
        for (int x = 0; x < m_Width; x++) {
            InventorySlot& slot = getInventorySlot(x, y);
            Item* pItem = slot.getItem();

            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX &&
                pItem->getItemType() == 0) {
                return true;

                // A present box is 2x2, so
                // advance x by one more.
                x += 1;
            }
        }
    }

    return false;
}
// #endif
//*/

void Inventory::clearQuestItem(list<Item*>& iList) {
    {
        list<Item*> ItemList;
        int height = getHeight();
        int width = getWidth();

        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                Item* pItem = getItem(i, j);
                if (pItem != NULL) {
                    // Look for the current item in the list of already checked items.
                    list<Item*>::iterator itr = find(ItemList.begin(), ItemList.end(), pItem);

                    if (itr == ItemList.end()) {
                        i += pItem->getVolumeWidth() - 1;

                        if (pItem->isQuestItem()) {
                            deleteItem(pItem->getObjectID());
                            iList.push_back(pItem);
                        } else {
                            // If the item is not in the list,
                            // put it into the list so that the same item
                            // is not checked twice.
                            ItemList.push_back(pItem);
                        }
                    }
                }
            }
        }
    }
}
