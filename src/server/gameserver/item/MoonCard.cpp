//////////////////////////////////////////////////////////////////////////////
// Filename    : MoonCard.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "MoonCard.h"

#include "Belt.h"
#include "DB.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Motorcycle.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Stash.h"
#include "Utility.h"
#include "Vampire.h"
#include "repository/ItemObjectRepository.h"

ItemID_t MoonCard::m_ItemIDRegistry = 0;
Mutex MoonCard::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class MoonCard member methods
//////////////////////////////////////////////////////////////////////////////

MoonCard::MoonCard()

{
    m_ItemType = 0;
}

MoonCard::MoonCard(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num)

{
    m_ItemType = itemType;
    m_Num = Num;

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "MoonCard::MoonCard() : Invalid item type or option type");
        throw Error("MoonCard::MoonCard() : Invalid item type or optionType");
    }
}

void MoonCard::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y, ItemID_t itemID)

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

    defaultItemObjectRepository().insertNumItem(GEAR_MOON_CARD, m_ItemID, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, (int)m_CreateType);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void MoonCard::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_MOON_CARD, field, m_ItemID);

    __END_CATCH
}

void MoonCard::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateNumItem(GEAR_MOON_CARD, m_ObjectID, m_ItemType, ownerID, (int)storage,
                                                storageID, (int)x, (int)y, (int)m_Num, m_ItemID);

    __END_CATCH
}

string MoonCard::toString() const

{
    StringStream msg;

    msg << "MoonCard(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Num:" << (int)m_Num << ")";

    return msg.toString();
}

VolumeWidth_t MoonCard::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOON_CARD, m_ItemType)->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t MoonCard::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOON_CARD, m_ItemType)->getVolumeHeight();

    __END_CATCH
}

Weight_t MoonCard::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_MOON_CARD, m_ItemType)->getWeight();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class MoonCardInfo member methods
//////////////////////////////////////////////////////////////////////////////

string MoonCardInfo::toString() const

{
    StringStream msg;
    msg << "MoonCardInfo(" << "ItemType:" << (int)m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << (int)m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << (int)m_Weight
        << ")";
    return msg.toString();
}

void MoonCardInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_MOON_CARD);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<BasicInfoRow> rows = defaultItemObjectRepository().loadBasicInfos(GEAR_MOON_CARD);

    for (size_t r = 0; r < rows.size(); r++) {
        MoonCardInfo* pMoonCardInfo = new MoonCardInfo();

        pMoonCardInfo->setItemType(rows[r].itemType);
        pMoonCardInfo->setName(rows[r].name);
        pMoonCardInfo->setEName(rows[r].ename);
        pMoonCardInfo->setPrice(rows[r].price);
        pMoonCardInfo->setVolumeType(rows[r].volume);
        pMoonCardInfo->setWeight(rows[r].weight);
        pMoonCardInfo->setRatio(rows[r].ratio);

        addItemInfo(pMoonCardInfo);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class MoonCardLoader member methods
//////////////////////////////////////////////////////////////////////////////

void MoonCardLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<NumObjectRow> rows = defaultItemObjectRepository().loadNumItemOfOwner(GEAR_MOON_CARD, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            MoonCard* pMoonCard = new MoonCard();

            pMoonCard->setItemID(rows[r].itemID);
            pMoonCard->setObjectID(rows[r].objectID);
            pMoonCard->setItemType(rows[r].itemType);

            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;

            pMoonCard->setNum(rows[r].num);
            pMoonCard->setCreateType((Item::CreateType)rows[r].createType);

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
                if (pInventory->canAddingEx(x, y, pMoonCard)) {
                    pInventory->addItemEx(x, y, pMoonCard);
                } else {
                    processItemBugEx(pCreature, pMoonCard);
                }
                break;

            case STORAGE_GEAR:
                processItemBugEx(pCreature, pMoonCard);
                break;

            case STORAGE_BELT:
                processItemBugEx(pCreature, pMoonCard);
                break;

            case STORAGE_EXTRASLOT:
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pMoonCard);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pMoonCard);
                else if (pCreature->isOusters())
                    pOusters->addItemToExtraInventorySlot(pMoonCard);
                break;

            case STORAGE_MOTORCYCLE:
                processItemBugEx(pCreature, pMoonCard);
                break;

            case STORAGE_STASH:
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pMoonCard);
                } else
                    pStash->insert(x, y, pMoonCard);
                break;

            case STORAGE_GARBAGE:
                processItemBug(pCreature, pMoonCard);
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

void MoonCardLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<NumZoneObjectRow> rows =
        defaultItemObjectRepository().loadNumItemInZone(GEAR_MOON_CARD, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        MoonCard* pMoonCard = new MoonCard();

        pMoonCard->setItemID(rows[r].itemID);
        pMoonCard->setObjectID(rows[r].objectID);
        pMoonCard->setItemType(rows[r].itemType);

        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;

        pMoonCard->setNum(rows[r].num);
        pMoonCard->setCreateType((Item::CreateType)rows[r].createType);

        switch (storage) {
        case STORAGE_ZONE: {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pMoonCard);
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

void MoonCardLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY

    Statement* pStmt;

    BEGIN_DB {}
    END_DB(pStmt)

    __END_CATCH
}
