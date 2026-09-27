//////////////////////////////////////////////////////////////////////////////
// Filename    : CarryingReceiver.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CarryingReceiver.h"

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

ItemID_t CarryingReceiver::m_ItemIDRegistry = 0;
Mutex CarryingReceiver::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
CarryingReceiver::CarryingReceiver()

{
    setItemType(0);
}

CarryingReceiver::CarryingReceiver(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    setItemType(itemType);
    setOptionType(optionType);

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "CarryingReceiver::CarryingReceiver() : Invalid item type or option type");
        throw Error("CarryingReceiver::CarryingReceiver() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void CarryingReceiver::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                              ItemID_t itemID)

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
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().insertOptionGradeItem(GEAR_CARRYING_RECEIVER, m_ItemID, m_ObjectID, getItemType(),
                                                        ownerID, (int)storage, storageID, (int)x, (int)y, optionField,
                                                        getGrade(), (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CarryingReceiver::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_CARRYING_RECEIVER, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void CarryingReceiver::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateAmulet(GEAR_CARRYING_RECEIVER, m_ObjectID, getItemType(), ownerID, (int)storage,
                                               storageID, (int)x, (int)y, optionField, getGrade(),
                                               (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CarryingReceiver::toString() const

{
    StringStream msg;

    msg << "CarryingReceiver(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string CarryingReceiverInfo::toString() const

{
    StringStream msg;

    msg << "CarryingReceiverInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",DefenseBonus:" << m_DefenseBonus << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void CarryingReceiverInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_CARRYING_RECEIVER);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoNoDurabilityRow> rows =
        defaultItemObjectRepository().loadGearInfosNoDurability(GEAR_CARRYING_RECEIVER);

    for (size_t r = 0; r < rows.size(); r++) {
        CarryingReceiverInfo* pCarryingReceiverInfo = new CarryingReceiverInfo();

        pCarryingReceiverInfo->setItemType(rows[r].itemType);
        pCarryingReceiverInfo->setName(rows[r].name);
        pCarryingReceiverInfo->setEName(rows[r].ename);
        pCarryingReceiverInfo->setPrice(rows[r].price);
        pCarryingReceiverInfo->setVolumeType(rows[r].volume);
        pCarryingReceiverInfo->setWeight(rows[r].weight);
        pCarryingReceiverInfo->setRatio(rows[r].ratio);
        pCarryingReceiverInfo->setDefenseBonus(rows[r].defense);
        pCarryingReceiverInfo->setProtectionBonus(rows[r].protection);
        pCarryingReceiverInfo->setReqAbility(rows[r].reqAbility);
        pCarryingReceiverInfo->setItemLevel(rows[r].itemLevel);
        pCarryingReceiverInfo->setDefaultOptions(rows[r].defaultOption);
        pCarryingReceiverInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pCarryingReceiverInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pCarryingReceiverInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pCarryingReceiverInfo->setNextItemType(rows[r].nextItemType);
        pCarryingReceiverInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pCarryingReceiverInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void CarryingReceiverLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<OptionGradeObjectRow> rows =
        defaultItemObjectRepository().loadOptionGradeOfOwner(GEAR_CARRYING_RECEIVER, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            CarryingReceiver* pCarryingReceiver = new CarryingReceiver();

            pCarryingReceiver->setItemID(rows[r].itemID);
            pCarryingReceiver->setObjectID(rows[r].objectID);
            pCarryingReceiver->setItemType(rows[r].itemType);

            if (de::gameContext()
                    .itemInfos()
                    .getItemInfo(Item::ITEM_CLASS_CARRYING_RECEIVER, pCarryingReceiver->getItemType())
                    ->isUnique())
                pCarryingReceiver->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pCarryingReceiver->setOptionType(optionTypes);

            pCarryingReceiver->setGrade(rows[r].grade);
            pCarryingReceiver->setEnchantLevel(rows[r].enchantLevel);
            pCarryingReceiver->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pCarryingReceiver)) {
                    pInventory->addItemEx(x, y, pCarryingReceiver);
                } else {
                    processItemBugEx(pCreature, pCarryingReceiver);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pCarryingReceiver);
                    } else {
                        processItemBugEx(pCreature, pCarryingReceiver);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pCarryingReceiver);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pCarryingReceiver);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pCarryingReceiver);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pCarryingReceiver);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pCarryingReceiver);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pCarryingReceiver);
                } else
                    pStash->insert(x, y, pCarryingReceiver);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pCarryingReceiver);
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
void CarryingReceiverLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to inventory
//--------------------------------------------------------------------------------
void CarryingReceiverLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    __END_CATCH
}
