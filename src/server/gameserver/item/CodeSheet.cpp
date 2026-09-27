//////////////////////////////////////////////////////////////////////////////
// Filename    : CodeSheet.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CodeSheet.h"

#include <algorithm>

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

void setStoneNum(vector<OptionType_t>& OptionType, CoordInven_t x, CoordInven_t y,
                 uint Num); // Defined in CGAddItemToCodeSheetHandler.cpp.

ItemID_t CodeSheet::m_ItemIDRegistry = 0;
Mutex CodeSheet::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
CodeSheet::CodeSheet()

    : m_ItemType(0) {}

CodeSheet::CodeSheet(ItemType_t itemType, const list<OptionType_t>& optionType)

    : m_ItemType(itemType), m_OptionType(optionType) {
    if (m_OptionType.size() == 0) {
        vector<OptionType_t> OptionType;
        while (OptionType.size() < 30) {
            OptionType.push_back((OptionType_t)0xff);
        }

        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < 6; ++j)
                if (((i + j) % 2) == 0) {
                    setStoneNum(OptionType, i, j, (rand() % 5) + 1);
                }

        copy(OptionType.begin(), OptionType.end(), back_inserter(m_OptionType));
    } else
        while (m_OptionType.size() < 30) {
            m_OptionType.push_back((OptionType_t)0xff);
        }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void CodeSheet::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    string optionField;
    setOptionTypeToField(m_OptionType, optionField);

    defaultItemObjectRepository().insertCodeSheet(GEAR_CODE_SHEET, m_ItemID, m_ObjectID, m_ItemType, ownerID,
                                                  (int)storage, storageID, (int)x, (int)y, optionField);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CodeSheet::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_CODE_SHEET, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CodeSheet::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(m_OptionType, optionField);

    defaultItemObjectRepository().updateCodeSheet(GEAR_CODE_SHEET, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                  storageID, (int)x, (int)y, optionField, m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CodeSheet::toString() const

{
    StringStream msg;

    msg << "CodeSheet(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType
        << ",OptionType:" << getOptionTypeToString(m_OptionType).c_str() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get width
//--------------------------------------------------------------------------------
VolumeWidth_t CodeSheet::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CODE_SHEET, m_ItemType)->getVolumeWidth();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get height
//--------------------------------------------------------------------------------
VolumeHeight_t CodeSheet::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CODE_SHEET, m_ItemType)->getVolumeHeight();

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get weight
//--------------------------------------------------------------------------------
Weight_t CodeSheet::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CODE_SHEET, m_ItemType)->getWeight();

    __END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CodeSheetInfo::toString() const

{
    StringStream msg;

    msg << "CodeSheetInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void CodeSheetInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_CODE_SHEET);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<HeadInfoRow> rows = defaultItemObjectRepository().loadHeadInfos(GEAR_CODE_SHEET);

    for (size_t r = 0; r < rows.size(); r++) {
        CodeSheetInfo* pCodeSheetInfo = new CodeSheetInfo();

        pCodeSheetInfo->setItemType(rows[r].itemType);
        pCodeSheetInfo->setName(rows[r].name);
        pCodeSheetInfo->setEName(rows[r].ename);
        pCodeSheetInfo->setPrice(rows[r].price);
        pCodeSheetInfo->setVolumeType(rows[r].volume);
        pCodeSheetInfo->setWeight(rows[r].weight);

        addItemInfo(pCodeSheetInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void CodeSheetLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<CodeSheetObjectRow> rows =
        defaultItemObjectRepository().loadCodeSheetOfOwner(GEAR_CODE_SHEET, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            CodeSheet* pCodeSheet = new CodeSheet();

            pCodeSheet->setItemID(rows[r].itemID);
            pCodeSheet->setObjectID(rows[r].objectID);
            pCodeSheet->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_CODE_SHEET, pCodeSheet->getItemType())
                    ->isUnique())
                pCodeSheet->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pCodeSheet->setOptionType(optionTypes);

            Inventory* pInventory = NULL;
            Slayer* pSlayer = NULL;
            Vampire* pVampire = NULL;
            Ousters* pOusters = NULL;
            Stash* pStash = NULL;

            if (pCreature->isSlayer()) {
                pSlayer = dynamic_cast<Slayer*>(pCreature);
                pInventory = pSlayer->getInventory();
                pStash = pSlayer->getStash();
            } else if (pCreature->isVampire()) {
                pVampire = dynamic_cast<Vampire*>(pCreature);
                pInventory = pVampire->getInventory();
                pStash = pVampire->getStash();
            } else if (pCreature->isOusters()) {
                pOusters = dynamic_cast<Ousters*>(pCreature);
                pInventory = pOusters->getInventory();
                pStash = pOusters->getStash();
            } else
                throw UnsupportedError("Saving Monster/NPC inventories is not supported.");

            switch (storage) {
            case STORAGE_INVENTORY:
                if (pInventory->canAddingEx(x, y, pCodeSheet)) {
                    pInventory->addItemEx(x, y, pCodeSheet);
                } else {
                    processItemBugEx(pCreature, pCodeSheet);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pCodeSheet);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pCodeSheet);
                break;

            case STORAGE_EXTRASLOT:
                processItemBugEx(pCreature, pCodeSheet);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pCodeSheet);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pCodeSheet);
                } else
                    pStash->insert(x, y, pCodeSheet);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pCodeSheet);
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
void CodeSheetLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_CODE_SHEET, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        CodeSheet* pCodeSheet = new CodeSheet();

        pCodeSheet->setItemID(rows[r].itemID);
        pCodeSheet->setObjectID(rows[r].objectID);
        pCodeSheet->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pCodeSheet->setOptionType(optionTypes);

        pCodeSheet->setDurability(rows[r].durability);
        pCodeSheet->setEnchantLevel(rows[r].enchantLevel);
        pCodeSheet->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pCodeSheet);
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
void CodeSheetLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
