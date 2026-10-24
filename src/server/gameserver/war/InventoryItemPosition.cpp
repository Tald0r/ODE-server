///////////////////////////////////////////////////////////////////
// InventoryItemPosition class implementation
///////////////////////////////////////////////////////////////////

#include "InventoryItemPosition.h"

#include "Assert.h"
#include "CreatureUtil.h"
#include "Effect.h"
#include "GCAddEffect.h"
#include "GCDeleteInventoryItem.h"
#include "GameContext.h"
#include "Inventory.h"
#include "Item.h"
#include "PCFinder.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "RelicUtil.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneGroup.h"

Item* InventoryItemPosition::popItem(bool bLock)

{
    if (bLock)
        return popItem_UNLOCKED();
    return popItem_LOCKED();
}

Item* InventoryItemPosition::popItem_LOCKED()

{
    Creature* pTargetCreature = findCreature();
    Zone* pZone = getZoneByCreature(pTargetCreature);
    if (pZone == NULL)
        return NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);
    Assert(pPC != NULL);

    return popItem_CORE(pPC);
}

Item* InventoryItemPosition::popItem_UNLOCKED()

{
    Creature* pTargetCreature = findCreature();

    if (pTargetCreature == NULL || !pTargetCreature->isPC())
        return NULL;

    Zone* pZone = getZoneByCreature(pTargetCreature);
    if (pZone == NULL)
        return NULL;

    Item* pItem = NULL;

    __ENTER_CRITICAL_SECTION((*pZone))

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);
    Assert(pPC != NULL);

    pItem = popItem_CORE(pPC);


    __LEAVE_CRITICAL_SECTION((*pZone))

    return pItem;
}

Item* InventoryItemPosition::popItemFrom(PlayerCreature& pc) {
    m_pZone = pc.getZone();
    m_bSetZone = true;

    return popItem_CORE(&pc);
}

Zone* InventoryItemPosition::getZone()

{
    if (m_bSetZone)
        return m_pZone;

    Creature* pCreature = findCreature();
    if (pCreature == NULL)
        return NULL;

    return getZoneByCreature(pCreature);
}

Creature* InventoryItemPosition::findCreature()

{
    Creature* pTargetCreature = NULL;

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    pTargetCreature = pcFinder.getCreature_LOCKED(m_OwnerName);
    if (pTargetCreature == NULL) {
        filelog("ItemError.log", "InventoryItemPosition:getItem() : No matching Creature.");

        return NULL;
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    return pTargetCreature;
}

Zone* InventoryItemPosition::getZoneByCreature(Creature* pCreature)

{
    if (m_bSetZone)
        return m_pZone;

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    m_bSetZone = true;
    m_pZone = pZone;

    return pZone;
}

Item* InventoryItemPosition::popItem_CORE(PlayerCreature* pPC)

{
    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    if (!pInventory->hasItem(m_InvenX, m_InvenY)) {
        filelog("ItemError.log", "InventoryItemPosition:getItem() : No item at that position.");

        return NULL;
    }

    Item* pItem = pInventory->getItem(m_InvenX, m_InvenY);
    Assert(pItem != NULL);

    if (!isExpectedItem(pItem->getItemClass(), pItem->getItemID())) {
        filelog("ItemError.log", "InventoryItemPosition:getItem() : another item is in that inventory slot");

        return NULL;
    }

    pInventory->deleteItem(pItem->getObjectID());

    GCDeleteInventoryItem gcDeleteInventoryItem;
    gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

    pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);

    if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE ||
        pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        deleteRelicEffect(pPC, pItem);
        sendBloodBibleEffect(pPC, Effect::EFFECT_CLASS_WARP_BLOOD_BIBLE_FROM_ME);
    }
    if (pItem->isFlagItem()) {
        Effect* pFlagEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_FLAG);
        if (pFlagEffect != NULL)
            pFlagEffect->setDeadline(0);
    }
    if (pItem->isSweeper()) {
        Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_SWEEPER);
        if (pEffect != NULL) {
            pEffect->setDeadline(0);
        }
    }
    if (pItem->getItemClass() == Item::ITEM_CLASS_WAR_ITEM) {
        deleteRelicEffect(pPC, pItem);
    }

    return pItem;
}

string InventoryItemPosition::toString() const

{
    return "InventoryItemPosition";
}
