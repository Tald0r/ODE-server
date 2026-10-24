//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTradeAddItemHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTradeAddItem.h"

#ifdef __GAME_SERVER__
#include "AR.h"
#include "Belt.h"
#include "FlagSet.h"
#include "GCTradeAddItem.h"
#include "GCTradeError.h"
#include "GCTradeVerify.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemUtil.h"
#include "OustersArmsband.h"
#include "PetItem.h"
#include "PlayerCreature.h"
#include "SG.h"
#include "SMG.h"
#include "SR.h"
#include "Store.h"
#include "TradeManager.h"
#include "Zone.h"
#include "trade/TradeTableContext.h"
#include "trade/TradeTableDecision.h"

namespace {

// A refusal answers the sender, either as the error the trade protocol has or
// as a verify packet, and may drop the sender's trade first.
void performRejection(CGTradeAddItem* pPacket, Player* pPlayer, TradeManager* pTradeManager, Creature* pSender,
                      const TradeTableRejection& rejection) {
    if (rejection.cancelSenderTrade)
        pTradeManager->cancelTrade(pSender);

    if (rejection.isTradeError) {
        CGTradeAddItemHandler::executeError(pPacket, pPlayer, rejection.code);
        return;
    }

    GCTradeVerify gcTradeVerify;
    gcTradeVerify.setCode(rejection.code);
    pPlayer->sendPacket(&gcTradeVerify);
}

// The body of all three race entry points. Only the slayer one consults the
// receiver's green-gift-box flag; the other two have never looked at it.
void applyAddItem(CGTradeAddItem* pPacket, Player* pPlayer, bool consultGreenGiftBoxFlag) {
    // The gate has already checked the pointers, so they are not checked
    // again here.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Zone* pZone = pPC->getZone();
    Creature* pTargetPC = pZone->getCreature(pPacket->getTargetObjectID());

    if (pTargetPC == NULL)
        return;

    PlayerCreature* pSender = dynamic_cast<PlayerCreature*>(pPC);
    PlayerCreature* pReceiver = dynamic_cast<PlayerCreature*>(pTargetPC);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    CoordInven_t X = 0;
    CoordInven_t Y = 0;
    Item* pItem = pSender->getInventory()->findItemOID(pPacket->getItemObjectID(), X, Y);

    TradeAddItemRequest request;
    request.senderObjectID = pSender->getObjectID();
    request.targetObjectID = pPacket->getTargetObjectID();
    request.itemFound = pItem != NULL;

    if (pItem != NULL) {
        request.itemTradeable = canTrade(pItem);
        request.itemInStore = pSender->getStore()->hasItem(pItem);
        request.itemIsEventGiftBox = pItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX;
        request.itemType = pItem->getItemType();
    }

    // Only a green gift box has the receiver inspected.
    if (request.itemIsEventGiftBox && request.itemType == 0) {
        Item* pExtraSlotItem = pReceiver->getExtraInventorySlotItem();

        request.receiverReceivedGreenGiftBox =
            consultGreenGiftBoxFlag && pReceiver->getFlagSet()->isOn(FLAGSET_RECEIVE_GREEN_GIFT_BOX);
        request.receiverHasRedGiftBox = pReceiver->getInventory()->hasRedGiftBox();
        request.receiverExtraSlotItemPresent = pExtraSlotItem != NULL;
        request.receiverExtraSlotIsRedGiftBox = pExtraSlotItem != NULL &&
                                                pExtraSlotItem->getItemClass() == Item::ITEM_CLASS_EVENT_GIFT_BOX &&
                                                pExtraSlotItem->getItemType() == 1;
    }

    ZoneTradeTableTopology topology(pTradeManager, pPC, pTargetPC);
    Outcome<TradeTableEvents, TradeTableRejection> outcome = decideTradeAddItem(request, topology);

    if (outcome.isRejected()) {
        performRejection(pPacket, pPlayer, pTradeManager, pPC, outcome.rejection());
        return;
    }

    const TradeTableEvents& events = outcome.events();

    for (TradeTableEvents::const_iterator itr = events.begin(); itr != events.end(); ++itr) {
        const TradeTableStep& step = (*itr);

        switch (step.action) {
        case TradeTableAction::SendVerify: {
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(step.code);
            pPlayer->sendPacket(&gcTradeVerify);
            break;
        }

        case TradeTableAction::StakeItem:
            topology.senderInfo()->addItem(pItem);
            break;

        case TradeTableAction::ResumeTrading:
            topology.senderInfo()->setStatus(TRADE_TRADING);
            topology.receiverInfo()->setStatus(TRADE_TRADING);
            break;

        case TradeTableAction::SendAddItem: {
            GCTradeAddItem gcTradeAddItem;
            CGTradeAddItemHandler::makeGCTradeAddItemPacket(&gcTradeAddItem, step.objectID, pItem, X, Y);
            pTargetPC->getPlayer()->sendPacket(&gcTradeAddItem);
            break;
        }

        // The remaining actions belong to the other two trade table requests.
        default:
            break;
        }
    }
}

} // namespace

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::execute(CGTradeAddItem* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    Creature* pPC = pGamePlayer->getCreature();
    Assert(pPC != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    Creature* pTargetPC = pZone->getCreature(pPacket->getTargetObjectID());

    ZoneTradeTableTopology topology(pTradeManager, pPC, pTargetPC);
    Outcome<void, TradeTableRejection> outcome =
        decideTradeTableGate(tradeTableGateOf(pPC, pTargetPC, pPacket->getTargetObjectID()), topology);

    if (outcome.isRejected()) {
        performRejection(pPacket, pPlayer, pTradeManager, pPC, outcome.rejection());
        return;
    }

    if (pPC->isSlayer())
        executeSlayer(pPacket, pPlayer);
    else if (pPC->isVampire())
        executeVampire(pPacket, pPlayer);
    else if (pPC->isOusters())
        executeOusters(pPacket, pPlayer);
    else
        throw ProtocolException("CGTradeAddItemHandler::execute() : Unknown player creature");

#endif

    __END_DEBUG_EX
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::executeSlayer(CGTradeAddItem* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyAddItem(pPacket, pPlayer, true);

#endif

    __END_DEBUG_EX
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::executeVampire(CGTradeAddItem* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyAddItem(pPacket, pPlayer, false);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::executeOusters(CGTradeAddItem* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyAddItem(pPacket, pPlayer, false);

#endif

    __END_DEBUG_EX
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::makeGCTradeAddItemPacket(GCTradeAddItem* pPacket, ObjectID_t Sender, Item* pItem,
                                                     CoordInven_t X, CoordInven_t Y) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    // The item information the receiver is given.
    pPacket->setTargetObjectID(Sender);
    pPacket->setItemObjectID(pItem->getObjectID());
    pPacket->setX(X);
    pPacket->setY(Y);
    pPacket->setItemClass(pItem->getItemClass());
    pPacket->setItemType(pItem->getItemType());
    pPacket->setOptionType(pItem->getOptionTypeList());
    pPacket->setDurability(pItem->getDurability());
    pPacket->setItemNum(pItem->getNum());
    pPacket->setSilver(pItem->getSilver());
    pPacket->setGrade(pItem->getGrade());
    pPacket->setEnchantLevel(pItem->getEnchantLevel());
    pPacket->clearList();

    // A few item classes need more work before their information goes out.
    Item::ItemClass IClass = pItem->getItemClass();

    if (IClass == Item::ITEM_CLASS_PET_ITEM) {
        PetItem* pPetItem = dynamic_cast<PetItem*>(pItem);

        if (pPetItem->getPetInfo() != NULL) {
            list<OptionType_t> olist;

            if (pPetItem->getPetInfo()->getPetOption() != 0)
                olist.push_back(pPetItem->getPetInfo()->getPetOption());

            pPacket->setOptionType(olist);
            pPacket->setDurability(pPetItem->getPetInfo()->getPetHP());
            pPacket->setEnchantLevel(pPetItem->getPetInfo()->getPetAttr());
            pPacket->setSilver(pPetItem->getPetInfo()->getPetAttrLevel());
            pPacket->setGrade((pPacket->getDurability() == 0)
                                  ? (pPetItem->getPetInfo()->getLastFeedTime().daysTo(VSDateTime::currentDateTime()))
                                  : (-1));
            pPacket->setItemNum(pPetItem->getPetInfo()->getPetLevel());
        }

    }
    // A gun carries its bullet count where other items carry their number.
    else if (IClass == Item::ITEM_CLASS_AR) {
        AR* pAR = dynamic_cast<AR*>(pItem);
        pPacket->setItemNum(pAR->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SR) {
        SR* pSR = dynamic_cast<SR*>(pItem);
        pPacket->setItemNum(pSR->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SG) {
        SG* pSG = dynamic_cast<SG*>(pItem);
        pPacket->setItemNum(pSG->getBulletCount());
    } else if (IClass == Item::ITEM_CLASS_SMG) {
        SMG* pSMG = dynamic_cast<SMG*>(pItem);
        pPacket->setItemNum(pSMG->getBulletCount());
    }
    // A belt carries the items in its pockets too.
    else if (IClass == Item::ITEM_CLASS_BELT) {
        Belt* pBelt = dynamic_cast<Belt*>(pItem);
        Inventory* pBeltInventory = pBelt->getInventory();

        for (int i = 0; i < pBelt->getPocketCount(); i++) {
            Item* pBeltItem = pBeltInventory->getItem(i, 0);
            if (pBeltItem != NULL) {
                SubItemInfo* pInfo = new SubItemInfo();
                pInfo->setObjectID(pBeltItem->getObjectID());
                pInfo->setItemClass(pBeltItem->getItemClass());
                pInfo->setItemType(pBeltItem->getItemType());
                pInfo->setItemNum(pBeltItem->getNum());
                pInfo->setSlotID(i);
                pPacket->addListElement(pInfo);
            }
        }
    }
    // So does an armsband.
    else if (IClass == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
        OustersArmsband* pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
        Inventory* pOustersArmsbandInventory = pOustersArmsband->getInventory();

        for (int i = 0; i < pOustersArmsband->getPocketCount(); i++) {
            Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem(i, 0);
            if (pOustersArmsbandItem != NULL) {
                SubItemInfo* pInfo = new SubItemInfo();
                pInfo->setObjectID(pOustersArmsbandItem->getObjectID());
                pInfo->setItemClass(pOustersArmsbandItem->getItemClass());
                pInfo->setItemType(pOustersArmsbandItem->getItemType());
                pInfo->setItemNum(pOustersArmsbandItem->getNum());
                pInfo->setSlotID(i);
                pPacket->addListElement(pInfo);
            }
        }
    }

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeAddItemHandler::executeError(CGTradeAddItem* pPacket, Player* pPlayer, BYTE ErrorCode) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    GCTradeError gcTradeError;
    gcTradeError.setTargetObjectID(pPacket->getTargetObjectID());
    gcTradeError.setCode(ErrorCode);
    pPlayer->sendPacket(&gcTradeError);

#endif

    __END_DEBUG_EX
}
