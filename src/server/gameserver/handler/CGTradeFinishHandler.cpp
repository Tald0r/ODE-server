//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTradeFinishHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTradeFinish.h"

#ifdef __GAME_SERVER__
#include "GCTradeError.h"
#include "GCTradeFinish.h"
#include "GCTradeVerify.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Slayer.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "ZoneUtil.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeFinishHandler::execute(CGTradeFinish* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    ObjectID_t TargetOID = pPacket->getTargetObjectID();
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    Creature* pPC = pGamePlayer->getCreature();
    Assert(pPC != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    // Look in the zone for the one wanted for the exchange.
    Creature* pTargetPC = NULL;

    // NoSuch removed.
    pTargetPC = pZone->getCreature(TargetOID);

    // It is an error when there is no exchange partner
    if (pTargetPC == NULL) {
        pTradeManager->cancelTrade(pPC);
        executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST);
        return;
    }

    // It is an error when the exchange partner is not a person or not of the same race.
    if (!pTargetPC->isPC() || !isSameRace(pTargetPC, pPC)) {
        pTradeManager->cancelTrade(pPC);
        executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_RACE_DIFFER);
        return;
    }

    // Check that both are in a safe zone.
    if (!isInSafeZone(pPC) || !isInSafeZone(pTargetPC)) {
        pTradeManager->cancelTrade(pPC);
        executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_NOT_SAFE);
        return;
    }

    // It is an error when a motorcycle is being ridden.
    if (pPC->isSlayer() && pTargetPC->isSlayer()) {
        Slayer* pSlayer1 = dynamic_cast<Slayer*>(pPC);
        Slayer* pSlayer2 = dynamic_cast<Slayer*>(pTargetPC);

        if (pSlayer1->hasRideMotorcycle() || pSlayer2->hasRideMotorcycle()) {
            pTradeManager->cancelTrade(pPC);
            executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_MOTORCYCLE);
            return;
        }
    }

    if (pPC->isOusters() && pTargetPC->isOusters()) {
        Ousters* pOusters1 = dynamic_cast<Ousters*>(pPC);
        Ousters* pOusters2 = dynamic_cast<Ousters*>(pTargetPC);

        if (pOusters1->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) ||
            pOusters2->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
            pTradeManager->cancelTrade(pPC);
            executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_MOTORCYCLE);
            return;
        }
    }

    // It is an error when the two are not in an exchange.
    if (!pTradeManager->isTrading(pPC, pTargetPC)) {
        pTradeManager->cancelTrade(pPC);
        executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_NOT_TRADING);
        return;
    }

    if (pPC->isSlayer())
        executeSlayer(pPacket, pPlayer);
    else if (pPC->isVampire())
        executeVampire(pPacket, pPlayer);
    else if (pPC->isOusters())
        executeOusters(pPacket, pPlayer);
    else
        throw ProtocolException("CGTradeFinishHandler::execute() : unknown player creature.");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeFinishHandler::executeSlayer(CGTradeFinish* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    // Errors were checked in the enclosing function, so
    // whether the pointer is null is not checked here.
    ObjectID_t TargetOID = pPacket->getTargetObjectID();
    BYTE CODE = pPacket->getCode();
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Zone* pZone = pPC->getZone();
    Creature* pTargetPC = pZone->getCreature(TargetOID);

    // NoSuch removed.
    if (pTargetPC == NULL)
        return;

    Slayer* pSender = dynamic_cast<Slayer*>(pPC);
    Slayer* pReceiver = dynamic_cast<Slayer*>(pTargetPC);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    TradeInfo* pInfo1 = pTradeManager->getTradeInfo(pSender->getName());
    TradeInfo* pInfo2 = pTradeManager->getTradeInfo(pReceiver->getName());
    Player* pTargetPlayer = pTargetPC->getPlayer();
    GCTradeFinish gcTradeFinish;

    // Get the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    // This is the code that accepts the exchange.
    if (CODE == CG_TRADE_FINISH_ACCEPT) {
        // It is an error when it is not time for an OK to arrive.
        if (pInfo1->isValidOKTime(currentTime) == false) {
            pTradeManager->cancelTrade(pPC);
            executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_UNKNOWN);
            return;
        }

        // Tell the other side that the exchange was accepted.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_ACCEPT);
        pTargetPlayer->sendPacket(&gcTradeFinish);

        // Change the exchange state.
        pInfo1->setStatus(TRADE_FINISH);


        // If the other side allows the exchange too, actually perform it.
        if (pInfo2->getStatus() == TRADE_FINISH) {
            // Exchange only when the exchange is certainly possible.
            if (pTradeManager->canTrade(pSender, pReceiver) == 1) {
                // Send both sides a packet telling them to perform the exchange.
                gcTradeFinish.setTargetObjectID(pSender->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pTargetPlayer->sendPacket(&gcTradeFinish);

                gcTradeFinish.setTargetObjectID(pReceiver->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pPlayer->sendPacket(&gcTradeFinish);

                // Actually perform the exchange.
                pTradeManager->processTrade(pSender, pReceiver);
            } else if (pTradeManager->canTrade(pSender, pReceiver) == 2) {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_EVENT_GIFT_BOX);
                return;
            } else {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_NOT_ENOUGH_SPACE);
                return;
            }
        }
    }
    // This is the code that refuses the exchange.
    else if (CODE == CG_TRADE_FINISH_REJECT) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_REJECT);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Cancel the exchange.
        pTradeManager->cancelTrade(pSender, pReceiver);

        // Tell the other side that the exchange was cancelled.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_REJECT);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // This is the code that reconsiders the exchange.
    else if (CODE == CG_TRADE_FINISH_RECONSIDER) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Change the exchange state.
        pInfo1->setStatus(TRADE_TRADING);

        // After OK and then cancel, no OK may arrive again for the next 4
        // seconds. So the time is set here.
        pInfo1->setNextTime(currentTime);

        // Tell the other side that the exchange was reconsidered.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_RECONSIDER);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // An unknown code. Cut it off neatly.
    else
        throw ProtocolException("CGTradeFinish::executeSlayer() : unknown code");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeFinishHandler::executeVampire(CGTradeFinish* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    // Errors were checked in the enclosing function, so
    // whether the pointer is null is not checked here.
    ObjectID_t TargetOID = pPacket->getTargetObjectID();
    BYTE CODE = pPacket->getCode();
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Zone* pZone = pPC->getZone();
    Creature* pTargetPC = pZone->getCreature(TargetOID);

    // NoSuch removed.
    if (pTargetPC == NULL)
        return;

    Vampire* pSender = dynamic_cast<Vampire*>(pPC);
    Vampire* pReceiver = dynamic_cast<Vampire*>(pTargetPC);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    TradeInfo* pInfo1 = pTradeManager->getTradeInfo(pSender->getName());
    TradeInfo* pInfo2 = pTradeManager->getTradeInfo(pReceiver->getName());
    Player* pTargetPlayer = pTargetPC->getPlayer();
    GCTradeFinish gcTradeFinish;

    // Get the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    // This is the code that accepts the exchange.
    if (CODE == CG_TRADE_FINISH_ACCEPT) {
        // It is an error when it is not time for an OK to arrive.
        if (pInfo1->isValidOKTime(currentTime) == false) {
            pTradeManager->cancelTrade(pPC);
            executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_UNKNOWN);
            return;
        }

        // Tell the other side that the exchange was accepted.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_ACCEPT);
        pTargetPlayer->sendPacket(&gcTradeFinish);

        // Change the exchange state.
        pInfo1->setStatus(TRADE_FINISH);

        // If the other side allows the exchange too, actually perform it.
        if (pInfo2->getStatus() == TRADE_FINISH) {
            // Exchange only when the exchange is certainly possible.
            if (pTradeManager->canTrade(pSender, pReceiver) == 1) {
                // Send both sides a packet telling them to perform the exchange.
                gcTradeFinish.setTargetObjectID(pSender->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pTargetPlayer->sendPacket(&gcTradeFinish);

                gcTradeFinish.setTargetObjectID(pReceiver->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pPlayer->sendPacket(&gcTradeFinish);

                // Actually perform the exchange.
                pTradeManager->processTrade(pSender, pReceiver);
            } else if (pTradeManager->canTrade(pSender, pReceiver) == 2) {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_EVENT_GIFT_BOX);
                return;
            } else {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_NOT_ENOUGH_SPACE);
                return;
            }
        }
    }
    // This is the code that refuses the exchange.
    else if (CODE == CG_TRADE_FINISH_REJECT) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_REJECT);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Cancel the exchange.
        pTradeManager->cancelTrade(pSender, pReceiver);

        // Tell the other side that the exchange was cancelled.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_REJECT);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // This is the code that reconsiders the exchange.
    else if (CODE == CG_TRADE_FINISH_RECONSIDER) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Change the exchange state.
        pInfo1->setStatus(TRADE_TRADING);

        // After OK and then cancel, no OK may arrive again for the next 4
        // seconds. So the time is set here.
        pInfo1->setNextTime(currentTime);

        // Tell the other side that the exchange was reconsidered.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_RECONSIDER);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // An unknown code. Cut it off neatly.
    else
        throw ProtocolException("CGTradeFinish::executeVampire() : unknown code");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeFinishHandler::executeOusters(CGTradeFinish* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    // Errors were checked in the enclosing function, so
    // whether the pointer is null is not checked here.
    ObjectID_t TargetOID = pPacket->getTargetObjectID();
    BYTE CODE = pPacket->getCode();
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    Zone* pZone = pPC->getZone();
    Creature* pTargetPC = pZone->getCreature(TargetOID);

    // NoSuch removed.
    if (pTargetPC == NULL)
        return;

    Ousters* pSender = dynamic_cast<Ousters*>(pPC);
    Ousters* pReceiver = dynamic_cast<Ousters*>(pTargetPC);

    TradeManager* pTradeManager = pZone->getTradeManager();
    Assert(pTradeManager != NULL);

    TradeInfo* pInfo1 = pTradeManager->getTradeInfo(pSender->getName());
    TradeInfo* pInfo2 = pTradeManager->getTradeInfo(pReceiver->getName());
    Player* pTargetPlayer = pTargetPC->getPlayer();
    GCTradeFinish gcTradeFinish;

    // Get the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    // This is the code that accepts the exchange.
    if (CODE == CG_TRADE_FINISH_ACCEPT) {
        // It is an error when it is not time for an OK to arrive.
        if (pInfo1->isValidOKTime(currentTime) == false) {
            pTradeManager->cancelTrade(pPC);
            executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_UNKNOWN);
            return;
        }

        // Tell the other side that the exchange was accepted.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_ACCEPT);
        pTargetPlayer->sendPacket(&gcTradeFinish);

        // Change the exchange state.
        pInfo1->setStatus(TRADE_FINISH);

        // If the other side allows the exchange too, actually perform it.
        if (pInfo2->getStatus() == TRADE_FINISH) {
            // Exchange only when the exchange is certainly possible.
            if (pTradeManager->canTrade(pSender, pReceiver) == 1) {
                // Send both sides a packet telling them to perform the exchange.
                gcTradeFinish.setTargetObjectID(pSender->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pTargetPlayer->sendPacket(&gcTradeFinish);

                gcTradeFinish.setTargetObjectID(pReceiver->getObjectID());
                gcTradeFinish.setCode(GC_TRADE_FINISH_EXECUTE);
                pPlayer->sendPacket(&gcTradeFinish);

                // Actually perform the exchange.
                pTradeManager->processTrade(pSender, pReceiver);
            } else if (pTradeManager->canTrade(pSender, pReceiver) == 2) {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_EVENT_GIFT_BOX);
                return;
            } else {
                pTradeManager->cancelTrade(pPC);
                executeError(pPacket, pPlayer, GC_TRADE_ERROR_CODE_NOT_ENOUGH_SPACE);
                return;
            }
        }
    }
    // This is the code that refuses the exchange.
    else if (CODE == CG_TRADE_FINISH_REJECT) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_REJECT);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Cancel the exchange.
        pTradeManager->cancelTrade(pSender, pReceiver);

        // Tell the other side that the exchange was cancelled.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_REJECT);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // This is the code that reconsiders the exchange.
    else if (CODE == CG_TRADE_FINISH_RECONSIDER) {
        // With OK currently pressed, a verify packet has to be sent to the client.
        if (pInfo1->getStatus() == TRADE_FINISH) {
            // Send the verify packet.
            GCTradeVerify gcTradeVerify;
            gcTradeVerify.setCode(GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER);
            pPlayer->sendPacket(&gcTradeVerify);
        }

        // Change the exchange state.
        pInfo1->setStatus(TRADE_TRADING);

        // After OK and then cancel, no OK may arrive again for the next 4
        // seconds. So the time is set here.
        pInfo1->setNextTime(currentTime);

        // Tell the other side that the exchange was reconsidered.
        gcTradeFinish.setTargetObjectID(pSender->getObjectID());
        gcTradeFinish.setCode(GC_TRADE_FINISH_RECONSIDER);
        pTargetPlayer->sendPacket(&gcTradeFinish);
    }
    // An unknown code. Cut it off neatly.
    else
        throw ProtocolException("CGTradeFinish::executeOusters() : unknown code");

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTradeFinishHandler::executeError(CGTradeFinish* pPacket, Player* pPlayer, BYTE ErrorCode)

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
