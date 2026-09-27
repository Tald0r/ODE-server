//////////////////////////////////////////////////////////////////////////////
// Filename    : Glove.cpp
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Glove.h"

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

ItemID_t Glove::m_ItemIDRegistry = 0;
Mutex Glove::m_Mutex;

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
Glove::Glove()

//: m_ItemType(0), m_Durability(0)
{
    setItemType(0);
    setDurability(0);
}

Glove::Glove(ItemType_t itemType, const list<OptionType_t>& optionType)

//: m_ItemType(itemType), m_OptionType(optionType), m_Durability(0)
{
    setItemType(itemType);
    setOptionType(optionType);

    setDurability(computeMaxDurability(this));

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), getItemType(), getOptionTypeList())) {
        filelog("itembug.log", "Glove::Glove() : Invalid item type or option type");
        throw Error("Glove::Glove() : Invalid item type or optionType");
    }
}


//--------------------------------------------------------------------------------
// create item
//--------------------------------------------------------------------------------
void Glove::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertGear(GEAR_GLOVE, m_ItemID, m_ObjectID, getItemType(), ownerID, (int)storage,
                                             storageID, (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)m_CreateType);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Glove::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_GLOVE, field, m_ItemID);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void Glove::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    string optionField;
    setOptionTypeToField(getOptionTypeList(), optionField);

    defaultItemObjectRepository().updateGear(GEAR_GLOVE, m_ObjectID, getItemType(), ownerID, (int)storage, storageID,
                                             (int)x, (int)y, optionField, getDurability(), (int)getGrade(),
                                             (int)getEnchantLevel(), m_ItemID);

    __END_CATCH
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string Glove::toString() const

{
    StringStream msg;

    msg << "Glove(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)getItemType()
        << ",OptionType:" << getOptionTypeToString(getOptionTypeList()).c_str()
        << ",Durability:" << (int)getDurability() << ",EnchantLevel:" << (int)getEnchantLevel() << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
string GloveInfo::toString() const

{
    StringStream msg;

    msg << "GloveInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",Description:" << m_Description << ",Durability:" << m_Durability << ",DefenseBonus:" << m_DefenseBonus
        << ")";

    return msg.toString();
}


//--------------------------------------------------------------------------------
// load from DB
//--------------------------------------------------------------------------------
void GloveInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_GLOVE);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<GearInfoRow> rows = defaultItemObjectRepository().loadGearInfos(GEAR_GLOVE);

    for (size_t r = 0; r < rows.size(); r++) {
        GloveInfo* pGloveInfo = new GloveInfo();

        pGloveInfo->setItemType(rows[r].itemType);
        pGloveInfo->setName(rows[r].name);
        pGloveInfo->setEName(rows[r].ename);
        pGloveInfo->setPrice(rows[r].price);
        pGloveInfo->setVolumeType(rows[r].volume);
        pGloveInfo->setWeight(rows[r].weight);
        pGloveInfo->setRatio(rows[r].ratio);
        pGloveInfo->setDurability(rows[r].durability);
        pGloveInfo->setDefenseBonus(rows[r].defense);
        pGloveInfo->setProtectionBonus(rows[r].protection);
        pGloveInfo->setReqAbility(rows[r].reqAbility);
        pGloveInfo->setItemLevel(rows[r].itemLevel);
        pGloveInfo->setDefaultOptions(rows[r].defaultOption);
        pGloveInfo->setUpgradeRatio(rows[r].upgradeRatio);
        pGloveInfo->setUpgradeCrashPercent(rows[r].upgradeCrashPercent);
        pGloveInfo->setNextOptionRatio(rows[r].nextOptionRatio);
        pGloveInfo->setNextItemType(rows[r].nextItemType);
        pGloveInfo->setDowngradeRatio(rows[r].downgradeRatio);

        addItemInfo(pGloveInfo);
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// load to creature
//--------------------------------------------------------------------------------
void GloveLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<GearObjectRow> rows = defaultItemObjectRepository().loadGearOfOwner(GEAR_GLOVE, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            Glove* pGlove = new Glove();

            pGlove->setItemID(rows[r].itemID);
            pGlove->setObjectID(rows[r].objectID);
            pGlove->setItemType(rows[r].itemType);

            if (de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_GLOVE, pGlove->getItemType())->isUnique())
                pGlove->setUnique();

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            string optionField = rows[r].optionField;
            list<OptionType_t> optionTypes;
            setOptionTypeFromField(optionTypes, optionField);
            pGlove->setOptionType(optionTypes);

            pGlove->setDurability(rows[r].durability);
            pGlove->setGrade(rows[r].grade);
            pGlove->setEnchantLevel(rows[r].enchantLevel);
            pGlove->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pGlove)) {
                    pInventory->addItemEx(x, y, pGlove);
                } else {
                    processItemBugEx(pCreature, pGlove);
                }
                break;

            case STORAGE_GEAR:
                if (pCreature->isSlayer()) {
                    if (!pSlayer->isWear((Slayer::WearPart)x)) {
                        pSlayer->wearItem((Slayer::WearPart)x, pGlove);
                    } else {
                        processItemBugEx(pCreature, pGlove);
                    }
                } else if (pCreature->isVampire()) {
                    processItemBugEx(pCreature, pGlove);
                }
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pGlove);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pGlove);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pGlove);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pGlove);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pGlove);
                } else
                    pStash->insert(x, y, pGlove);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pGlove);
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
void GloveLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<GearZoneObjectRow> rows =
        defaultItemObjectRepository().loadGearInZone(GEAR_GLOVE, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        Glove* pGlove = new Glove();

        pGlove->setItemID(rows[r].itemID);
        pGlove->setObjectID(rows[r].objectID);
        pGlove->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        string optionField = rows[r].optionField;
        list<OptionType_t> optionTypes;
        setOptionTypeFromField(optionTypes, optionField);
        pGlove->setOptionType(optionTypes);

        pGlove->setDurability(rows[r].durability);
        pGlove->setEnchantLevel(rows[r].enchantLevel);
        pGlove->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pGlove);
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
void GloveLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
