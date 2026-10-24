//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTradePrepareHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTradePrepare.h"

#ifdef __GAME_SERVER__
#include "CreatureUtil.h"
#include "GCTradeError.h"
#include "GCTradePrepare.h"
#include "GCTradeVerify.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "StringStream.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "ZoneUtil.h"
#include "trade/TradePrepareDecision.h"

namespace {

// The zone's trade records, as the decision asks about them.
class ZoneTradePrepareTopology : public TradePrepareTopology {
public:
    ZoneTradePrepareTopology(TradeManager* pTradeManager, Creature* pSender, Creature* pReceiver)
        : m_pTradeManager(pTradeManager), m_pSender(pSender), m_pReceiver(pReceiver) {}

    bool senderHasTradeInfo() override {
        return m_pTradeManager->hasTradeInfo(m_pSender->getName());
    }

    bool receiverHasTradeInfo() override {
        return m_pTradeManager->hasTradeInfo(m_pReceiver->getName());
    }

    bool isTrading() override {
        return m_pTradeManager->isTrading(m_pSender, m_pReceiver);
    }

private:
    TradeManager* m_pTradeManager;
    Creature* m_pSender;
    Creature* m_pReceiver;
};

// A slayer on a motorcycle and an ousters with a summoned sylph both refuse
// to trade; a vampire is never mounted.
bool isMounted(Creature* pCreature) {
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        return pSlayer->hasRideMotorcycle();
    }

    if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        return pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH);
    }

    return false;
}

} // namespace

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradePrepareHandler::execute(CGTradePrepare* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    Creature* pSender = pGamePlayer->getCreature();
    Assert(pSender != NULL);

    Zone* pZone = pSender->getZone();
    Assert(pZone != NULL);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    Creature* pReceiver = pZone->getCreature(pPacket->getTargetObjectID());

    TradePrepareRequest request;
    request.code = pPacket->getCode();
    request.senderObjectID = pSender->getObjectID();
    request.targetObjectID = pPacket->getTargetObjectID();
    request.targetExists = pReceiver != NULL;

    if (pReceiver != NULL) {
        const bool sameRace = isSameRace(pSender, pReceiver);

        request.targetIsSelf = pSender->getName() == pReceiver->getName();
        request.targetIsSameRacePC = pReceiver->isPC() && sameRace;
        request.bothInSafeZone = isInSafeZone(pSender) && isInSafeZone(pReceiver);
        // Only a pair of the same race is ever asked about a mount.
        request.senderMounted = sameRace && isMounted(pSender);
        request.receiverMounted = sameRace && isMounted(pReceiver);
    }

    ZoneTradePrepareTopology topology(pTradeManager, pSender, pReceiver);
    Outcome<TradePrepareEvents, TradePrepareRejection> outcome = decideTradePrepare(request, topology);

    if (outcome.isRejected()) {
        const TradePrepareRejection& rejection = outcome.rejection();

        // One character logged in twice would trade items between its own
        // two copies, so the connection is cut.
        if (rejection.reason == TradePrepareReason::SameCreature) {
            StringStream msg;
            msg << "CGTradePrepare : Error, Same Creature!!! Name[" << pSender->getName() << "]";
            filelog("TradeError.log", "%s", msg.toString().c_str());
            throw ProtocolException(msg.toString());
        }

        if (rejection.reason == TradePrepareReason::UnknownCode)
            throw ProtocolException("CGTradePrepare::execute() : Unknown Code");

        if (rejection.cancel == TradeCancel::Sender)
            pTradeManager->cancelTrade(pSender);

        if (rejection.isTradeError) {
            executeError(pPacket, pPlayer, rejection.code);
        } else {
            GCTradePrepare gcTradePrepare;
            gcTradePrepare.setTargetObjectID(rejection.sendObjectID);
            gcTradePrepare.setCode(rejection.code);
            pPlayer->sendPacket(&gcTradePrepare);
        }

        return;
    }

    const TradePrepareEvents& events = outcome.events();

    if (events.sendPrepare) {
        GCTradePrepare gcTradePrepare;
        gcTradePrepare.setTargetObjectID(events.sendObjectID);
        gcTradePrepare.setCode(events.sendCode);

        if (events.sendTo == TradePeer::Receiver)
            pReceiver->getPlayer()->sendPacket(&gcTradePrepare);
        else
            pPlayer->sendPacket(&gcTradePrepare);
    }

    if (events.initTrade)
        pTradeManager->initTrade(pSender, pReceiver);

    if (events.cancel == TradeCancel::Sender)
        pTradeManager->cancelTrade(pSender);
    else if (events.cancel == TradeCancel::Pair)
        pTradeManager->cancelTrade(pSender, pReceiver);

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradePrepareHandler::executeError(CGTradePrepare* pPacket, Player* pPlayer, BYTE ErrorCode)

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
