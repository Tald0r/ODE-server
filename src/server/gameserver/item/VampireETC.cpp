//////////////////////////////////////////////////////////////////////////////
// Filename    : VampireETC.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampireETC.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t VampireETC::m_ItemIDRegistry = 0;
Mutex VampireETC::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
VampireETC::VampireETC()

{
    m_ItemType = 0;
}

VampireETC::VampireETC(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    m_ItemType = itemType;
    m_Num = 1;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "VampireETC::VampireETC() : Invalid item type or option type");
        throw Error("VampireETC::VampireETC() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void VampireETC::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumOnlyItem(GEAR_VAMPIRE_ETC, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                    (int)storage, storageID, (int)x, (int)y, (int)m_Num);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireETC::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_ETC, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampireETC::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumOnlyItem(GEAR_VAMPIRE_ETC, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                    storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireETC::toString() const

{
    StringStream msg;
    msg << "VampireETC(" << "ItemID:" << m_ItemID << ",ItemType:" << m_ItemType << ",Num:" << (int)m_Num << ")";
    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t VampireETC::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_ETC, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t VampireETC::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_ETC, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t VampireETC::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_ETC, m_ItemType)->getWeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string VampireETCInfo::toString() const

{
    StringStream msg;

    msg << "VampireETCInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void VampireETCInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_ETC);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<StringInfoRow> rows = defaultItemObjectRepository().loadStringInfos(GEAR_VAMPIRE_ETC);

    for (size_t r = 0; r < rows.size(); r++) {
        VampireETCInfo* pVampireETCInfo = new VampireETCInfo();

        pVampireETCInfo->setItemType(rows[r].basic.itemType);
        pVampireETCInfo->setName(rows[r].basic.name);
        pVampireETCInfo->setEName(rows[r].basic.ename);
        pVampireETCInfo->setPrice(rows[r].basic.price);
        pVampireETCInfo->setVolumeType(rows[r].basic.volume);
        pVampireETCInfo->setWeight(rows[r].basic.weight);
        pVampireETCInfo->setRatio(rows[r].basic.ratio);
        pVampireETCInfo->setReqAbility(rows[r].value);

        addItemInfo(pVampireETCInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void VampireETCLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumOnlyObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemOfOwner(GEAR_VAMPIRE_ETC, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            VampireETC* pVampireETC = new VampireETC();

            pVampireETC->setItemID(rows[r].itemID);
            pVampireETC->setObjectID(rows[r].objectID);
            pVampireETC->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pVampireETC->setNum(rows[r].num);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Stash* pStash = NULL;

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
                if (pInventory->canAddingEx(x, y, pVampireETC)) {
                    pInventory->addItemEx(x, y, pVampireETC);
                } else {
                    processItemBugEx(pCreature, pVampireETC);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pVampireETC);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pVampireETC);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampireETC);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampireETC);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pVampireETC);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampireETC);
                } else
                    pStash->insert(x, y, pVampireETC);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pVampireETC);
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
void VampireETCLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumOnlyZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumOnlyItemInZone(GEAR_VAMPIRE_ETC, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        VampireETC* pVampireETC = new VampireETC();

        pVampireETC->setItemID(rows[r].itemID);
        pVampireETC->setObjectID(rows[r].objectID);
        pVampireETC->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pVampireETC->setNum(rows[r].num);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampireETC);
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
void VampireETCLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
