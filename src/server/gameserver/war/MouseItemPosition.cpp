///////////////////////////////////////////////////////////////////
// MouseItemPosition class implementation
///////////////////////////////////////////////////////////////////

#include "MouseItemPosition.h"

#include "Assert.h"
#include "CreatureUtil.h"
#include "Effect.h"
#include "GCAddEffect.h"
#include "GCDeleteInventoryItem.h"
#include "GameContext.h"
#include "Item.h"
#include "PCFinder.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "RelicUtil.h"
#include "Utility.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneUtil.h"

Item* MouseItemPosition::popItem(bool bLock)

{
    if (bLock)
        return popItem_UNLOCKED();
    return popItem_LOCKED();
}

Item* MouseItemPosition::popItem_UNLOCKED()

{
    Creature* pTargetCreature = findCreature();
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

Item* MouseItemPosition::popItem_LOCKED()

{
    Creature* pTargetCreature = findCreature();

    Zone* pZone = getZoneByCreature(pTargetCreature);
    Assert(pZone != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pTargetCreature);
    Assert(pPC != NULL);

    return popItem_CORE(pPC);
}

Item* MouseItemPosition::popItemFrom(PlayerCreature& pc) {
    m_pZone = pc.getZone();
    m_bSetZone = true;

    return popItem_CORE(&pc);
}

Zone* MouseItemPosition::getZone()

{
    // Return the one already obtained, if there is one.
    if (m_bSetZone)
        return m_pZone;

    // If there is none, obtain a new one.
    Creature* pTargetCreature = findCreature();

    return getZoneByCreature(pTargetCreature);
}

Item* MouseItemPosition::popItem_CORE(PlayerCreature* pPC)

{
    Item* pItem;

    if (pPC->getExtraInventorySlotItem() == NULL) {
        filelog("ItemError.log", "InventoryItemPosition:getItem() : No item at that position.");

        return NULL;
    }

    pItem = pPC->getExtraInventorySlotItem();

    if (!isExpectedItem(pItem->getItemClass(), pItem->getItemID())) {
        filelog("ItemError.log", "MouseItemPosition:getItem() : the player holds another item on the mouse");

        return NULL;
    }

    pPC->deleteItemFromExtraInventorySlot();

    GCDeleteInventoryItem gcDeleteInventoryItem;
    gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

    pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);

    if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE ||
        pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        sendBloodBibleEffect(pPC, Effect::EFFECT_CLASS_WARP_BLOOD_BIBLE_FROM_ME);
        deleteRelicEffect(pPC, pItem);
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

Creature* MouseItemPosition::findCreature()

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

Zone* MouseItemPosition::getZoneByCreature(Creature* pCreature)

{
    if (pCreature == NULL)
        return NULL;

    Assert(pCreature->isPC());

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    m_bSetZone = true;
    m_pZone = pZone;

    return pZone;
}

string MouseItemPosition::toString() const

{
    return "MouseItemPosition";
}
