//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSilverCoatingHandler.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSilverCoating.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "NPC.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "Vampire.h"

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSilverCoatingHandler::execute(CGSilverCoating* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    ObjectID_t ITEMOID = pPacket->getObjectID();
    Creature* pPC = dynamic_cast<GamePlayer*>(pPlayer)->getCreature();
    bool bSlayer = true;
    Gold_t playerMoney = 0;
    Price_t coatingPrice = 0;
    Item* pItem = NULL;
    Slayer* pSlayer = NULL;
    Vampire* pVampire = NULL;
    int storage = 0;
    int X = 0;
    int Y = 0;
    GCNPCResponse response;

    // Tell whether the player is a Slayer or a Vampire.
    if (pPC->isSlayer())
        bSlayer = true;
    else if (pPC->isVampire())
        bSlayer = false;

    // Check that the player holds the item it wants to coat
    if (bSlayer) {
        pSlayer = dynamic_cast<Slayer*>(pPC);
        playerMoney = pSlayer->getGold();
        pItem = pSlayer->findItemOID(ITEMOID, storage, X, Y);
    } else {
        pVampire = dynamic_cast<Vampire*>(pPC);
        playerMoney = pVampire->getGold();
        pItem = pVampire->findItemOID(ITEMOID, storage, X, Y);
    }

    // With no item there is of course nothing to coat.
    if (pItem == NULL) {
        response.setCode(NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_NOT_EXIST);
        pPlayer->sendPacket(&response);
        return;
    }

    // When the item to coat cannot be coated...
    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
        break;
    default:
        response.setCode(NPC_RESPONSE_SILVER_COATING_FAIL_ITEM_TYPE);
        pPlayer->sendPacket(&response);
        return;
    }

    coatingPrice = de::gameContext().prices().getSilverCoatingPrice(pItem);
    if (coatingPrice > playerMoney) {
        response.setCode(NPC_RESPONSE_SILVER_COATING_FAIL_MONEY);
        pPlayer->sendPacket(&response);
        return;
    }

    // Get the maximum silver plating and... plate it.
    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());
    pItem->setSilver(pItemInfo->getMaxSilver());

    // Take the money.
    if (bSlayer) {
        // by sigi. 2002.9.4
        pSlayer->decreaseGoldEx(coatingPrice);
    } else {
        // by sigi. 2002.9.4
        pVampire->decreaseGoldEx(coatingPrice);
    }

    // Only silver has to be saved.
    // Item save optimization.
    char pField[80];
    snprintf(pField, sizeof(pField), "Silver=%d", pItem->getSilver());
    pItem->tinysave(pField);

    // Save to the DB that the item was coated with silver.
    // STORAGE_STASH can certainly come back, but
    // repairing something in the stash makes no sense, so
    // it is not saved.

    // Send the OK packet.
    response.setCode(NPC_RESPONSE_SILVER_COATING_OK);
    response.setParameter(playerMoney - coatingPrice);
    pPlayer->sendPacket(&response);

#endif

    __END_DEBUG_EX __END_CATCH
}
