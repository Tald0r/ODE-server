//////////////////////////////////////////////////////////////////////////////
// Filename    : Motorcycle.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Motorcycle.h"

#include <memory>

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

ItemID_t Motorcycle::m_ItemIDRegistry = 0;
Mutex Motorcycle::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Motorcycle::Motorcycle()

    : m_ItemType(0), m_Durability(0), m_pInventory(NULL) {}

Motorcycle::Motorcycle(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_OptionType(optionType), m_Durability(0), m_pInventory(NULL) {
    // A motorcycle's inventory size depends on its type.
    switch (itemType) {
    case 0:
        m_pInventory = new Inventory(10, 6);
        break;
    case 1:
        m_pInventory = new Inventory(10, 6);
        break;
    case 2:
        m_pInventory = new Inventory(10, 6);
        break;
    case 3:
        m_pInventory = new Inventory(10, 6);
        break;
    case 4:
        m_pInventory = new Inventory(10, 6);
        break;
    case 5:
        m_pInventory = new Inventory(10, 6);
        break;
    default:
        m_pInventory = new Inventory(10, 6);
        break;
    }

    m_Durability = computeMaxDurability(this);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "Motorcycle::Motorcycle() : Invalid item type or option type");
        throw Error("Motorcycle::Motorcycle() : Invalid item type or optionType");
    }
}

Motorcycle::~Motorcycle()

{
    SAFE_DELETE(m_pInventory);
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Motorcycle::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

{
    if (itemID == 0) {
        __ENTER_CRITICAL_SECTION(m_Mutex)

        m_ItemIDRegistry += de::gameContext().itemInfos().getItemIDSuccessor();
        m_ItemID = m_ItemIDRegistry;

        __LEAVE_CRITICAL_SECTION(m_Mutex)
    } else {
        m_ItemID = itemID;
    }

    string optionField;
    setOptionTypeToField(m_OptionType, optionField);

    defaultItemObjectRepository().insertMotorcycle(GEAR_MOTORCYCLE, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                   (int)storage, storageID, (int)x, (int)y, optionField, m_Durability);
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Motorcycle::tinysave(const char* field) const

{
    defaultItemObjectRepository().tinysaveGear(GEAR_MOTORCYCLE, field, m_ItemID);
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Motorcycle::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    string optionField;
    setOptionTypeToField(m_OptionType, optionField);

    defaultItemObjectRepository().updateMotorcycle(GEAR_MOTORCYCLE, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                   storageID, (int)x, (int)y, optionField, m_Durability, m_ItemID);
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Motorcycle::toString() const

{
    StringStream msg;

    msg << "Motorcycle(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType
        << ",OptionType:" << getOptionTypeToString(m_OptionType).c_str() << ",Durability:" << (int)m_Durability << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t Motorcycle::getVolumeWidth() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOTORCYCLE, m_ItemType)->getVolumeWidth();
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t Motorcycle::getVolumeHeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOTORCYCLE, m_ItemType)->getVolumeHeight();
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t Motorcycle::getWeight() const

{
    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOTORCYCLE, m_ItemType)->getWeight();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string MotorcycleInfo::toString() const

{
    StringStream msg;

    msg << "MotorcycleInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void MotorcycleInfoManager::load()

{
    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MOTORCYCLE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<DurabilityInfoRow> rows = defaultItemObjectRepository().loadDurabilityInfos(GEAR_MOTORCYCLE);

    for (size_t r = 0; r < rows.size(); r++) {
        MotorcycleInfo* pMotorcycleInfo = new MotorcycleInfo();

        pMotorcycleInfo->setItemType(rows[r].itemType);
        pMotorcycleInfo->setName(rows[r].name);
        pMotorcycleInfo->setEName(rows[r].ename);
        pMotorcycleInfo->setPrice(rows[r].price);
        pMotorcycleInfo->setVolumeType(rows[r].volume);
        pMotorcycleInfo->setWeight(rows[r].weight);
        pMotorcycleInfo->setRatio(rows[r].ratio);
        pMotorcycleInfo->setDurability(rows[r].durability);

        addItemInfo(pMotorcycleInfo);
    }
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void MotorcycleLoader::load(Creature* pCreature)

{
    Assert(pCreature != NULL);

    vector<MotorcycleObjectRow> rows =
        defaultItemObjectRepository().loadMotorcycleOfOwner(GEAR_MOTORCYCLE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            // Nothing attaches the motorcycle to the creature, so it is dropped after its row is parsed.
            auto pMotorcycle = std::make_unique<Motorcycle>();

            pMotorcycle->setItemID(rows[r].itemID);
            pMotorcycle->setObjectID(rows[r].objectID);
            pMotorcycle->setItemType(rows[r].itemType);

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pMotorcycle->setOptionType(optionTypes);

            pMotorcycle->setDurability(rows[r].durability);


        } catch (Error& error) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), error.toString().c_str());
            throw;
        } catch (Throwable& t) {
            filelog("itemLoadError.txt", "[%s] %s", getItemClassName().c_str(), t.toString().c_str());
        }
    }
}


//--------------------------------------------------------------------------------
// load to zone
//--------------------------------------------------------------------------------
void MotorcycleLoader::load(Zone* pZone)

{
    Assert(pZone != NULL);

    vector<MotorcycleZoneObjectRow> rows =
        defaultItemObjectRepository().loadMotorcycleInZone(GEAR_MOTORCYCLE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Motorcycle* pMotorcycle = new Motorcycle();

        pMotorcycle->setItemID(rows[r].itemID);
        pMotorcycle->setObjectID(rows[r].objectID);
        pMotorcycle->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pMotorcycle->setDurability(rows[r].durability);

        switch (storage) {
        case STORAGE_ZONE: {
            pZone->addItem(pMotorcycle, x, y);
        } break;

        case STORAGE_STASH:
        case STORAGE_CORPSE:
            throw UnsupportedError("Saving items inside boxes or corpses is not supported.");

        default:
            throw Error("Storage must be STORAGE_ZONE");
        }
    }
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void MotorcycleLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)
}
