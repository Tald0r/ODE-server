#include "GlobalItemPositionLoader.h"

#include "CorpseItemPosition.h"
#include "InventoryItemPosition.h"
#include "MouseItemPosition.h"
#include "WarZoneRouting.h"
#include "ZoneItemPosition.h"
#include "repository/ItemRepository.h"

GlobalItemPosition* GlobalItemPositionLoader::load(Item::ItemClass itemClass, ItemID_t itemID)

{
    ItemPositionRow row;
    if (!defaultItemRepository().loadItemPosition(ItemObjectTableName[(int)itemClass], itemID, row))
        return NULL;

    return makeGlobalItemPosition(row);
}

GlobalItemPosition* GlobalItemPositionLoader::makeGlobalItemPosition(const ItemPositionRow& row)

{
    switch ((Storage)row.storage) {
    case STORAGE_INVENTORY: {
        InventoryItemPosition* pIIP = new InventoryItemPosition();
        pIIP->setOwnerName(row.ownerID);
        pIIP->setInventoryX(row.x);
        pIIP->setInventoryY(row.y);
        return (GlobalItemPosition*)pIIP;
    } break;
    case STORAGE_EXTRASLOT: {
        MouseItemPosition* pMIP = new MouseItemPosition();
        pMIP->setOwnerName(row.ownerID);
        return (GlobalItemPosition*)pMIP;
    } break;
    case STORAGE_ZONE: {
        ZoneItemPosition* pZIP = new ZoneItemPosition();
        pZIP->setZoneID((StorageID_t)row.storageID);
        pZIP->setZoneX(row.x);
        pZIP->setZoneY(row.y);
        return (GlobalItemPosition*)pZIP;
    } break;
    case STORAGE_CORPSE: {
        // A corpse's zone is the row's OwnerID, as text.
        CorpseItemPosition* pCIP = new CorpseItemPosition();
        pCIP->setZoneID(de::war::corpseZoneIDOf(row.ownerID));
        pCIP->setCorpseObjectID((StorageID_t)row.storageID);
        pCIP->setObjectID(row.objectID);

        return (GlobalItemPosition*)pCIP;
    } break;

    default:
        return NULL;
        break;
    }
}
