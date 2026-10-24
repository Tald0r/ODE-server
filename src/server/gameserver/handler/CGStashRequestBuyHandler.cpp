//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStashRequestBuyHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGStashRequestBuy.h"

#ifdef __GAME_SERVER__
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "Stash.h"
#include "Vampire.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGStashRequestBuyHandler::execute(CGStashRequestBuy* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    PlayerCreature* pPlayerCreature = dynamic_cast<PlayerCreature*>(pPC);

    BYTE curStashNum = pPlayerCreature->getStashNum();

    // Return when the number of stashes held is at the maximum
    if (curStashNum >= STASH_RACK_MAX) {
        GCNPCResponse failpkt;
        failpkt.setCode(NPC_RESPONSE_STASH_SELL_FAIL_MAX);
        pPlayer->sendPacket(&failpkt);
        return;
    }

    Price_t price = de::gameContext().prices().getStashPrice(curStashNum + 1);

    // Not enough money is a failure too.
    if (pPlayerCreature->getGold() < price) {
        GCNPCResponse failpkt;
        failpkt.setCode(NPC_RESPONSE_STASH_SELL_FAIL_MONEY);
        pPlayer->sendPacket(&failpkt);
        return;
    }

    // Raise the stash count by one, and...
    pPlayerCreature->setStashNumEx(curStashNum + 1);

    // Take the money.

    // by sigi. 2002.9.4
    pPlayerCreature->decreaseGoldEx(price);

    // finally send the OK packet.
    GCNPCResponse okpkt;
    okpkt.setCode(NPC_RESPONSE_STASH_SELL_OK);
    pPlayer->sendPacket(&okpkt);

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
