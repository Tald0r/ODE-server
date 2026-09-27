//////////////////////////////////////////////////////////////////////////////
// Filename    : Key.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Key.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t Key::m_ItemIDRegistry = 0;
Mutex Key::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Key::Key()

    : m_ItemType(0), m_Target(0) {}

Key::Key(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_Target(0) {
    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Key::Key() : Invalid item type or option type");
        throw Error("Key::Key() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Key::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

{
    __BEGIN_TRY

    if (itemID == 0) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        m_ItemIDRegistry += de::gameContext().itemInfos().getItemIDSuccessor();
        m_ItemID = m_ItemIDRegistry;

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } else {
        m_ItemID = itemID;
    }

    defaultItemObjectRepository().insertKey(GEAR_KEY, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                            storageID, (int)x, (int)y, m_Target);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Key::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_KEY, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Key::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateKey(GEAR_KEY, m_ObjectID, m_ItemType, ownerID, (int)storage, storageID, (int)x,
                                            (int)y, m_Target, m_ItemID);

    __END_CATCH
}

ItemID_t Key::setNewMotorcycle(Slayer* pSlayer) {
    __BEGIN_TRY

    ItemID_t targetID = 0;

    // Even when the target is not 0, a new motorcycle has to be put in if the target is missing.
    Assert(pSlayer != NULL);
    Zone* pZone = pSlayer->getZone();
    Assert(pZone != NULL);

    KeyInfo* pKeyInfo =
        dynamic_cast<KeyInfo*>(de::gameContext().itemInfos().getItemInfo(getItemClass(), getItemType()));
    Assert(pKeyInfo != NULL);

    list<OptionType_t> option;
    ItemType_t motorcycleType = pKeyInfo->getTargetType();

    if (pKeyInfo->getOptionType() != 0)
        option.push_back(pKeyInfo->getOptionType());

    Item* pMotorcycle =
        de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_MOTORCYCLE, motorcycleType, option);
    Assert(pMotorcycle != NULL);
    (pZone->getObjectRegistry()).registerObject(pMotorcycle);

    pMotorcycle->create(pSlayer->getName(), STORAGE_ZONE, pZone->getZoneID(), pSlayer->getX(), pSlayer->getY());
    setTarget(pMotorcycle->getItemID());

    targetID = pMotorcycle->getItemID();

    // The target ID has to be updated in the database as well.
    defaultItemObjectRepository().saveKeyTarget(GEAR_KEY, targetID, getItemID());

    // log
    filelog("motorcycle.txt", "[SetTargetID] Owner = %s, KeyID = %lu, Key's targetID = %lu, MotorcycleID = %lu",
            pSlayer->getName().c_str(), getItemID(), getTarget(), pMotorcycle->getItemID());

    // pMotorcycle could be used below, but it is deleted here so the existing code stays untouched.
    SAFE_DELETE(pMotorcycle);

    return targetID;

    __END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Key::toString() const

{
    StringStream msg;

    msg << "Key(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Target:" << (int)m_Target << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Key::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_KEY, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Key::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_KEY, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Key::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_KEY, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string KeyInfo::toString() const

{
    StringStream msg;

    msg << "KeyInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName << ",Price:" << m_Price
        << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight << ",Description:" << m_Description
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void KeyInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_KEY);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<IntPairInfoRow> rows = defaultItemObjectRepository().loadIntPairInfos(GEAR_KEY);

    for (size_t r = 0; r < rows.size(); r++) {
        KeyInfo* pKeyInfo = new KeyInfo();

        pKeyInfo->setItemType(rows[r].basic.itemType);
        pKeyInfo->setName(rows[r].basic.name);
        pKeyInfo->setEName(rows[r].basic.ename);
        pKeyInfo->setPrice(rows[r].basic.price);
        pKeyInfo->setVolumeType(rows[r].basic.volume);
        pKeyInfo->setWeight(rows[r].basic.weight);
        pKeyInfo->setRatio(rows[r].basic.ratio);
        pKeyInfo->setOptionType(rows[r].first);
        pKeyInfo->setTargetType(rows[r].second);

        addItemInfo(pKeyInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void KeyLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<KeyObjectRow> rows = defaultItemObjectRepository().loadKeyOfOwner(GEAR_KEY, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Key* pKey = new Key();

            pKey->setItemID(rows[r].itemID);
            pKey->setObjectID(rows[r].objectID);
            pKey->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            StorageID_t storageID = rows[r].storageID;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pKey->setTarget(rows[r].target);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Item* pItem = NULL;
            Stash* pStash = NULL;
            Belt* pBelt = NULL;
            Inventory* pBeltInventory = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
                pStash = pSlayer->getStash();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
                pStash = pVampire->getStash();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pKey)) {
                    pInventory->addItemEx(x, y, pKey);
                } else {
                    processItemBugEx(pCreature, pKey);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pKey);
                break;

            case STORAGE_BELT:
                if (pCreature->isSlayer()) {
                    pItem = pSlayer->findBeltIID(storageID);
                    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                        pBelt = dynamic_cast<Belt*>(pItem);
                        pBeltInventory = pBelt->getInventory();
                        if (pBeltInventory->canAddingEx(x, 0, pKey)) {
                            pBeltInventory->addItem(x, 0, pKey);
                        } else {
                            processItemBugEx(pCreature, pKey);
                        }
                    } else {
                        processItemBugEx(pCreature, pKey);
                    }
                }
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pKey);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pKey);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pKey);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pKey);
                } else
                    pStash->insert(x, y, pKey);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pKey);
                break;

            default:
                throw Error("invalid storage or OwnerID must be NULL");
            }

        } catch (Error& error) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), error.toString().c_str());
            throw;
        } catch (Throwable& t) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), t.toString().c_str());
        }
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void KeyLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<KeyZoneObjectRow> rows =
        defaultItemObjectRepository().loadKeyInZone(GEAR_KEY, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Key* pKey = new Key();

        pKey->setItemID(rows[r].itemID);
        pKey->setObjectID(rows[r].objectID);
        pKey->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pKey->setTarget(rows[r].target);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pKey);
        } break;

        case STORAGE_STASH:
        case STORAGE_CORPSE:
            throw UnsupportedError("Saving items inside boxes or corpses is not supported.");

        default:
            throw Error("Storage must be STORAGE_ZONE");
        }
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void KeyLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
