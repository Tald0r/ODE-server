//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTradeRemoveItemHandler.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTradeRemoveItem.h"

#ifdef __GAME_SERVER__
#include "GCTradeError.h"
#include "GCTradeRemoveItem.h"
#include "GCTradeVerify.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "PlayerCreature.h"
#include "TradeManager.h"
#include "Zone.h"
#include "trade/TradeTableContext.h"
#include "trade/TradeTableDecision.h"

namespace {

// A refusal answers the sender and may drop the sender's trade first.
void performRejection(CGTradeRemoveItem* pPacket, Player* pPlayer, TradeManager* pTradeManager, Creature* pSender,
                      const TradeTableRejection& rejection) {
    if (rejection.cancelSenderTrade)
        pTradeManager->cancelTrade(pSender);

    if (rejection.isTradeError) {
        CGTradeRemoveItemHandler::executeError(pPacket, pPlayer, rejection.code);
        return;
    }

    GCTradeVerify gcTradeVerify;
    gcTradeVerify.setCode(rejection.code);
    pPlayer->sendPacket(&gcTradeVerify);
}

// The body of all three race entry points. Only the slayer one pushes the
// sender's next allowed OK out.
void applyRemoveItem(CGTradeRemoveItem* pPacket, Player* pPlayer, bool delayOK) {
    // The gate has already checked the pointers, so they are not checked
    // again here.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Zone* pZone = pPC->getZone();
    Creature* pTargetPC = pZone->getCreature(pPacket->getTargetObjectID());

    if (pTargetPC == NULL)
        return;

    PlayerCreature* pSender = dynamic_cast<PlayerCreature*>(pPC);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    CoordInven_t X = 0;
    CoordInven_t Y = 0;
    Item* pItem = pSender->getInventory()->findItemOID(pPacket->getItemObjectID(), X, Y);

    TradeRemoveItemRequest request;
    request.senderObjectID = pSender->getObjectID();
    request.targetObjectID = pPacket->getTargetObjectID();
    request.itemFound = pItem != NULL;
    request.delayOK = delayOK;

    ZoneTradeTableTopology topology(pTradeManager, pPC, pTargetPC);
    Outcome<TradeTableEvents, TradeTableRejection> outcome = decideTradeRemoveItem(request, topology);

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

        case TradeTableAction::UnstakeItem:
            topology.senderInfo()->removeItem(pItem);
            break;

        case TradeTableAction::DelayOK: {
            Timeval currentTime;
            getCurrentTime(currentTime);
            topology.senderInfo()->setNextTime(currentTime);
            break;
        }

        case TradeTableAction::ResumeTrading:
            topology.senderInfo()->setStatus(TRADE_TRADING);
            topology.receiverInfo()->setStatus(TRADE_TRADING);
            break;

        case TradeTableAction::SendRemoveItem: {
            GCTradeRemoveItem gcTradeRemoveItem;
            gcTradeRemoveItem.setTargetObjectID(step.objectID);
            gcTradeRemoveItem.setItemObjectID(pItem->getObjectID());
            pTargetPC->getPlayer()->sendPacket(&gcTradeRemoveItem);
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
void CGTradeRemoveItemHandler::execute(CGTradeRemoveItem* pPacket, Player* pPlayer)

{
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
        throw ProtocolException("CGTradeRemoveItem::execute() : Unknown player creature");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeRemoveItemHandler::executeSlayer(CGTradeRemoveItem* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        applyRemoveItem(pPacket, pPlayer, true);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeRemoveItemHandler::executeVampire(CGTradeRemoveItem* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        applyRemoveItem(pPacket, pPlayer, false);

#endif

    __END_DEBUG_EX
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeRemoveItemHandler::executeOusters(CGTradeRemoveItem* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        applyRemoveItem(pPacket, pPlayer, false);

#endif

    __END_DEBUG_EX
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeRemoveItemHandler::executeError(CGTradeRemoveItem* pPacket, Player* pPlayer, BYTE ErrorCode)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        GCTradeError gcTradeError;
    gcTradeError.setTargetObjectID(pPacket->getTargetObjectID());
    gcTradeError.setCode(ErrorCode);
    pPlayer->sendPacket(&gcTradeError);

#endif

    __END_DEBUG_EX
}
