//////////////////////////////////////////////////////////////////////////////
// Filename    : VampirePortalItem.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "VampirePortalItem.h"

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
#include "skill/EffectVampirePortal.h"

ItemID_t VampirePortalItem::m_ItemIDRegistry = 0;
Mutex VampirePortalItem::m_Mutex;

//////////////////////////////////////////////////////////////////////////////
// class VampirePortalItem member methods
//////////////////////////////////////////////////////////////////////////////

VampirePortalItem::VampirePortalItem()

{
    __BEGIN_TRY

    m_ItemType = 0;
    m_Charge = 0;
    m_ZoneID = 0;
    m_X = 0;
    m_Y = 0;

    __END_CATCH
}

VampirePortalItem::VampirePortalItem(ItemType_t itemType, const list<OptionType_t>& optionType)

{
    __BEGIN_TRY

    m_ItemType = itemType;
    m_Charge = getMaxCharge();

    switch (m_ItemType) {
    case 3:
    case 4:
    case 5:
        m_ZoneID = 1003;
        m_X = 50;
        m_Y = 70;
        break;
    case 6:
    case 7:
    case 8:
        m_ZoneID = 1007;
        m_X = 62;
        m_Y = 65;
        break;
    case 9:
    case 10:
    case 11:
        m_ZoneID = 61;
        m_X = 102;
        m_Y = 220;
        break;
    default:
        m_ZoneID = 0;
        m_X = 0;
        m_Y = 0;
        break;
    }

    if (!de::gameContext().itemInfos().isPossibleItem(getItemClass(), m_ItemType, optionType)) {
        filelog("itembug.log", "VampirePortalItem::VampirePortalItem() : Invalid item type or option type");
        throw Error("VampirePortalItem::VampirePortalItem() : Invalid item type or optionType");
    }


    __END_CATCH
}

VampirePortalItem::~VampirePortalItem()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void VampirePortalItem::create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
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

    defaultItemObjectRepository().insertVampirePortal(GEAR_VAMPIRE_PORTAL_ITEM, m_ItemID, m_ObjectID, m_ItemType,
                                                      ownerID, (int)storage, storageID, (int)x, (int)y, m_Charge,
                                                      (int)m_ZoneID, (int)m_X, (int)m_Y);

    __END_CATCH
}

//--------------------------------------------------------------------------------
// save item
//--------------------------------------------------------------------------------
void VampirePortalItem::tinysave(const char* field) const

{
    __BEGIN_TRY

    defaultItemObjectRepository().tinysaveGear(GEAR_VAMPIRE_PORTAL_ITEM, field, m_ItemID);

    __END_CATCH
}

void VampirePortalItem::save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y)

{
    __BEGIN_TRY

    defaultItemObjectRepository().updateVampirePortal(GEAR_VAMPIRE_PORTAL_ITEM, m_ObjectID, m_ItemType, ownerID,
                                                      (int)storage, storageID, (int)x, (int)y, m_Charge, (int)m_ZoneID,
                                                      (int)m_X, (int)m_Y, m_ItemID);

    __END_CATCH
}

VolumeWidth_t VampirePortalItem::getVolumeWidth() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM, m_ItemType)
        ->getVolumeWidth();

    __END_CATCH
}

VolumeHeight_t VampirePortalItem::getVolumeHeight() const

{
    __BEGIN_TRY

    return de::gameContext()
        .itemInfos()
        .getItemInfo(Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM, m_ItemType)
        ->getVolumeHeight();

    __END_CATCH
}

Weight_t VampirePortalItem::getWeight() const

{
    __BEGIN_TRY

    return de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM, m_ItemType)->getWeight();

    __END_CATCH
}

string VampirePortalItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "VampirePortalItem(" << "ItemID:" << m_ItemID << ",ItemType:" << (int)m_ItemType << ",Charge:" << m_Charge
        << ",TargetZID:" << (int)m_ZoneID << ",TargetX:" << (int)m_X << ",TargetY:" << (int)m_Y << ")";

    return msg.toString();

    __END_CATCH
}

int VampirePortalItem::getMaxCharge(void) const

{
    __BEGIN_TRY

    VampirePortalItemInfo* pInfo = dynamic_cast<VampirePortalItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM, m_ItemType));
    Assert(pInfo != NULL);
    return pInfo->getMaxCharge();

    __END_CATCH
}

Durability_t VampirePortalItem::getDurability() const

{
    __BEGIN_TRY

    WORD highBits = m_X << 8;
    WORD lowBits = m_Y;
    return (WORD)(highBits | lowBits);

    __END_CATCH
}

Silver_t VampirePortalItem::getSilver() const

{
    __BEGIN_TRY

    return m_ZoneID;

    __END_CATCH
}

EnchantLevel_t VampirePortalItem::getEnchantLevel() const

{
    __BEGIN_TRY

    return m_Charge;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// class VampirePortalItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

void VampirePortalItemInfoManager::load()

{
    __BEGIN_TRY

    m_InfoCount = defaultItemObjectRepository().loadMaxGearType(GEAR_VAMPIRE_PORTAL_ITEM);

    m_pItemInfos = new ItemInfo*[m_InfoCount + 1];

    for (uint i = 0; i <= m_InfoCount; i++)
        m_pItemInfos[i] = NULL;

    vector<LevelStringInfoRow> rows = defaultItemObjectRepository().loadLevelStringInfos(GEAR_VAMPIRE_PORTAL_ITEM);

    for (size_t r = 0; r < rows.size(); r++) {
        VampirePortalItemInfo* pVampirePortalItemInfo = new VampirePortalItemInfo();

        pVampirePortalItemInfo->setItemType(rows[r].basic.itemType);
        pVampirePortalItemInfo->setName(rows[r].basic.name);
        pVampirePortalItemInfo->setEName(rows[r].basic.ename);
        pVampirePortalItemInfo->setPrice(rows[r].basic.price);
        pVampirePortalItemInfo->setVolumeType(rows[r].basic.volume);
        pVampirePortalItemInfo->setWeight(rows[r].basic.weight);
        pVampirePortalItemInfo->setRatio(rows[r].basic.ratio);
        pVampirePortalItemInfo->setMaxCharge(rows[r].itemLevel);
        pVampirePortalItemInfo->setReqAbility(rows[r].value);

        addItemInfo(pVampirePortalItemInfo);
    }

    __END_CATCH
}

string VampirePortalItemInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "VampirePortalItemInfo(" << "ItemType:" << m_ItemType << ",Name:" << m_Name << ",EName:" << m_EName
        << ",Price:" << m_Price << ",VolumeType:" << Volume2String[m_VolumeType] << ",Weight:" << m_Weight
        << ",MaxCharge:" << m_MaxCharge << ")";
    return msg.toString();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// class VampirePortalItemLoader member methods
//////////////////////////////////////////////////////////////////////////////

void VampirePortalItemLoader::load(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    vector<VampirePortalObjectRow> rows =
        defaultItemObjectRepository().loadVampirePortalOfOwner(GEAR_VAMPIRE_PORTAL_ITEM, pCreature->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        try {
            ItemID_t itemID = rows[r].itemID;
            ObjectID_t objectID = rows[r].objectID;
            ItemType_t itemType = rows[r].itemType;
            Storage storage = (Storage)rows[r].storage;
            BYTE x = rows[r].x;
            BYTE y = rows[r].y;
            int charge = rows[r].charge;
            ZoneID_t TargetZID = rows[r].targetZoneID;
            ZoneCoord_t TargetX = rows[r].targetX;
            ZoneCoord_t TargetY = rows[r].targetY;

            VampirePortalItem* pVampirePortalItem = new VampirePortalItem();
            pVampirePortalItem->setItemID(itemID);
            pVampirePortalItem->setObjectID(objectID);
            pVampirePortalItem->setItemType(itemType);
            pVampirePortalItem->setCharge(charge);
            pVampirePortalItem->setZoneID(TargetZID);
            pVampirePortalItem->setX(TargetX);
            pVampirePortalItem->setY(TargetY);

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

            if (storage == STORAGE_INVENTORY) {
                if (pInventory->canAddingEx(x, y, pVampirePortalItem)) {
                    pInventory->addItemEx(x, y, pVampirePortalItem);
                } else {
                    processItemBugEx(pCreature, pVampirePortalItem);
                }
            } else if (storage == STORAGE_GEAR) {
                processItemBugEx(pCreature, pVampirePortalItem);
            } else if (storage == STORAGE_BELT) {
                processItemBugEx(pCreature, pVampirePortalItem);
            } else if (storage == STORAGE_EXTRASLOT) {
                if (pCreature->isSlayer())
                    pSlayer->addItemToExtraInventorySlot(pVampirePortalItem);
                else if (pCreature->isVampire())
                    pVampire->addItemToExtraInventorySlot(pVampirePortalItem);
            } else if (storage == STORAGE_MOTORCYCLE) {
                processItemBugEx(pCreature, pVampirePortalItem);
            } else if (storage == STORAGE_STASH) {
                if (pStash->isExist(x, y)) {
                    processItemBugEx(pCreature, pVampirePortalItem);
                } else
                    pStash->insert(x, y, pVampirePortalItem);
            } else if (storage == STORAGE_GARBAGE) {
                processItemBug(pCreature, pVampirePortalItem);
            } else {
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

void VampirePortalItemLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    Assert(pZone != NULL);

    vector<VampirePortalObjectRow> rows = defaultItemObjectRepository().loadVampirePortalInZone(
        GEAR_VAMPIRE_PORTAL_ITEM, (int)STORAGE_ZONE, pZone->getZoneID());

    for (size_t r = 0; r < rows.size(); r++) {
        ItemID_t itemID = rows[r].itemID;
        ObjectID_t objectID = rows[r].objectID;
        ItemType_t itemType = rows[r].itemType;
        Storage storage = (Storage)rows[r].storage;
        BYTE x = rows[r].x;
        BYTE y = rows[r].y;
        int charge = rows[r].charge;
        ZoneID_t TargetZID = rows[r].targetZoneID;
        ZoneCoord_t TargetX = rows[r].targetX;
        ZoneCoord_t TargetY = rows[r].targetY;

        VampirePortalItem* pVampirePortalItem = new VampirePortalItem();
        pVampirePortalItem->setItemID(itemID);
        pVampirePortalItem->setObjectID(objectID);
        pVampirePortalItem->setItemType(itemType);
        pVampirePortalItem->setCharge(charge);
        pVampirePortalItem->setZoneID(TargetZID);
        pVampirePortalItem->setX(TargetX);
        pVampirePortalItem->setY(TargetY);

        if (storage == STORAGE_ZONE) {
            Tile& pTile = pZone->getTile(x, y);
            Assert(!pTile.hasItem());
            pTile.addItem(pVampirePortalItem);
        } else {
            throw Error("Storage must be STORAGE_ZONE");
        }
    }

    __END_CATCH
}

void VampirePortalItemLoader::load(StorageID_t storageID, Inventory* pInventory)

{
    __BEGIN_TRY
    __END_CATCH
}
