//////////////////////////////////////////////////////////////////////////////
// Filename    : CGExchangeBuyHandler.cpp
// Written By  : Exchange System
// Description : Handler for CGExchangeBuy
//////////////////////////////////////////////////////////////////////////////

#include "CGExchangeBuy.h"
#include "GCExchangeBuy.h"

#ifdef __GAME_SERVER__
#include <string>

#include "../server/gameserver/exchange/ExchangeService.h"
#include "GamePlayer.h"
#include "PlayerCreature.h"
#endif

void CGExchangeBuyHandler::execute(CGExchangeBuy* pPacket, Player* pPlayer) {
#ifdef __GAME_SERVER__

    // Validate player
    if (pPlayer == NULL)
        return;

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    PlayerCreature* pPC = pGamePlayer == NULL ? NULL : dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    if (pPC == NULL)
        return;

    // Call service to buy
    Outcome<ExchangePurchase, ExchangeRejection> result =
        ExchangeService::buyListing(pPC, pPacket->getListingID(), pPacket->getIdempotencyKey());

    // Send response
    GCExchangeBuy gcPacket;

    if (result.isOk()) {
        const ExchangePurchase& purchase = result.events();
        gcPacket.setSuccess(true);
        // The order id travels in its own field; the message is the success
        // text, as a refusal's is its reason's.
        gcPacket.setMessage(formatExchangeError(EXCHANGE_SUCCESS));
        gcPacket.setOrderID(purchase.orderID);
    } else {
        // A refusal puts the reason's text in m_Message and leaves the id 0.
        gcPacket.setSuccess(false);
        gcPacket.setMessage(result.rejection().message());
    }

    pPlayer->sendPacket(&gcPacket);

#endif // __GAME_SERVER__
}
