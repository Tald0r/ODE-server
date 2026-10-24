//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTradeMoneyHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTradeMoney.h"

#ifdef __GAME_SERVER__
#include "GCTradeError.h"
#include "GCTradeMoney.h"
#include "GCTradeVerify.h"
#include "GamePlayer.h"
#include "PlayerCreature.h"
#include "TradeManager.h"
#include "Zone.h"
#include "trade/TradeTableContext.h"
#include "trade/TradeTableDecision.h"

namespace {

// A refusal answers the sender and may drop the sender's trade first.
void performRejection(CGTradeMoney* pPacket, Player* pPlayer, TradeManager* pTradeManager, Creature* pSender,
                      const TradeTableRejection& rejection) {
    if (rejection.cancelSenderTrade)
        pTradeManager->cancelTrade(pSender);

    if (rejection.isTradeError) {
        CGTradeMoneyHandler::executeError(pPacket, pPlayer, rejection.code);
        return;
    }

    GCTradeVerify gcTradeVerify;
    gcTradeVerify.setCode(rejection.code);
    pPlayer->sendPacket(&gcTradeVerify);
}

// The body of all three race entry points; they have never differed.
void applyMoney(CGTradeMoney* pPacket, Player* pPlayer) {
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

    TradeMoneyRequest request;
    request.senderObjectID = pSender->getObjectID();
    request.targetObjectID = pPacket->getTargetObjectID();
    request.code = pPacket->getCode();
    request.amount = pPacket->getAmount();
    request.senderGold = pSender->getGold();
    request.receiverGold = pReceiver->getGold();

    ZoneTradeTableTopology topology(pTradeManager, pPC, pTargetPC);
    Outcome<TradeTableEvents, TradeTableRejection> outcome = decideTradeMoney(request, topology);

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

        case TradeTableAction::SetSenderGold:
            pSender->setGold(step.amount);
            break;

        case TradeTableAction::SetStakedGold:
            topology.senderInfo()->setGold(step.amount);
            break;

        case TradeTableAction::ResumeTrading:
            topology.senderInfo()->setStatus(TRADE_TRADING);
            topology.receiverInfo()->setStatus(TRADE_TRADING);
            break;

        case TradeTableAction::SendMoney: {
            GCTradeMoney gcTradeMoney;
            gcTradeMoney.setTargetObjectID(step.objectID);
            gcTradeMoney.setCode(step.code);
            gcTradeMoney.setAmount(step.amount);

            if (step.sendTo == TradePeer::Receiver)
                pTargetPC->getPlayer()->sendPacket(&gcTradeMoney);
            else
                pPlayer->sendPacket(&gcTradeMoney);

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
void CGTradeMoneyHandler::execute(CGTradeMoney* pPacket, Player* pPlayer)

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
        throw ProtocolException("CGTradeMoney::execute() : Unknown player creature");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeMoneyHandler::executeSlayer(CGTradeMoney* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyMoney(pPacket, pPlayer);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeMoneyHandler::executeVampire(CGTradeMoney* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyMoney(pPacket, pPlayer);

#endif

    __END_DEBUG_EX
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeMoneyHandler::executeOusters(CGTradeMoney* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    applyMoney(pPacket, pPlayer);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeMoneyHandler::executeError(CGTradeMoney* pPacket, Player* pPlayer, BYTE ErrorCode)

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
