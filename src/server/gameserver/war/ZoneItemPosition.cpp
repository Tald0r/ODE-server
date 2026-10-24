///////////////////////////////////////////////////////////////////
// ZoneItemPosition class implementation
///////////////////////////////////////////////////////////////////

#include "ZoneItemPosition.h"

#include "Assert.h"
#include "Corpse.h"
#include "Effect.h"
#include "GCAddEffectToTile.h"
#include "GCDeleteObject.h"
#include "Item.h"
#include "StringStream.h"
#include "Tile.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneUtil.h"

Item* ZoneItemPosition::getItem_LOCKED(bool isDel)

{
    Zone* pZone = getZone();
    if (pZone == NULL) {
        filelog("ItemError.log", "ZoneItemPosition:getItem() : Invalid ZoneID.");
        return NULL;
    }

    Item* pItem = getItemFromZone(pZone);
    if (pItem == NULL)
        return NULL;
    if (isDel)
        deleteItemFromZone(pZone, pItem);

    if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE ||
        pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        GCAddEffectToTile gcAddEffectToTile;
        gcAddEffectToTile.setEffectID(Effect::EFFECT_CLASS_WARP_BLOOD_BIBLE_FROM_TILE);
        gcAddEffectToTile.setXY(m_ZoneX, m_ZoneY);

        pZone->broadcastPacket(m_ZoneX, m_ZoneY, &gcAddEffectToTile);
    }

    return pItem;
}

Item* ZoneItemPosition::getItem(bool isDel)

{
    Zone* pZone = getZone();
    if (pZone == NULL) {
        filelog("ItemError.log", "ZoneItemPosition:getItem() : Invalid ZoneID.");
        return NULL;
    }

    Item* pItem = NULL;

    __ENTER_CRITICAL_SECTION((*pZone))

    pItem = getItemFromZone(pZone);

    if (pItem == NULL) {
        return NULL;
    }

    if (isDel) {
        deleteItemFromZone(pZone, pItem);
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE ||
        pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        GCAddEffectToTile gcAddEffectToTile;
        gcAddEffectToTile.setEffectID(Effect::EFFECT_CLASS_WARP_BLOOD_BIBLE_FROM_TILE);
        gcAddEffectToTile.setXY(m_ZoneX, m_ZoneY);

        pZone->broadcastPacket(m_ZoneX, m_ZoneY, &gcAddEffectToTile);
    }

    __LEAVE_CRITICAL_SECTION((*pZone))

    return pItem;
}

Item* ZoneItemPosition::popItem(bool bLock)

{
    if (bLock)
        return getItem(true);
    return getItem_LOCKED(true);
}

Item* ZoneItemPosition::getItemFromZone(Zone* pZone)

{
    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    if (!rect.ptInRect(m_ZoneX, m_ZoneY)) {
        filelog("ItemError.log", "ZoneItemPosition:getItem() : Invalid item coordinates.");

        return NULL;
    }

    Tile& tile = pZone->getTile(m_ZoneX, m_ZoneY);

    if (!tile.hasItem()) {
        filelog("ItemError.log", "ZoneItemPosition:getItem() : No item.");

        return NULL;
    }

    Item* pItem = tile.getItem();
    if (!isExpectedItem(pItem->getItemClass(), pItem->getItemID())) {
        filelog("ItemError.log", "ZoneItemPosition:getItem() : another item lies where the row says (%s)",
                toString().c_str());

        return NULL;
    }

    return pItem;
}

void ZoneItemPosition::deleteItemFromZone(Zone* pZone, Item* pItem)

{
    pZone->deleteItem(pItem, m_ZoneX, m_ZoneY);

    GCDeleteObject gcDeleteObject(pItem->getObjectID());
    pZone->broadcastPacket(m_ZoneX, m_ZoneY, &gcDeleteObject);
}

string ZoneItemPosition::toString() const

{
    StringStream msg;

    msg << "ZoneItemPosition(" << "ZoneID:" << (int)m_ZoneID << ",ZoneX:" << (int)m_ZoneX << ",ZoneY:" << (int)m_ZoneY
        << ")";

    return msg.toString();
}
