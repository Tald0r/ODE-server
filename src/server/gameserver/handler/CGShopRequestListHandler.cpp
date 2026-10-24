//////////////////////////////////////////////////////////////////////////////
// Filename    : CGShopRequestListHandler.cpp
// Description : Handler for CGShopRequestList.
//////////////////////////////////////////////////////////////////////////////

#include "CGShopRequestList.h"

#ifdef __GAME_SERVER__
#include "GCNPCResponse.h"
#include "GCShopList.h"
#include "GCShopListMysterious.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "quest/Action.h"
#include "quest/Condition.h"
#include "quest/Trigger.h"
#include "quest/TriggerManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGShopRequestListHandler::execute(CGShopRequestList* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // Pull the information out of the packet.
    ObjectID_t NPCID = pPacket->getObjectID();
    ShopRackType_t type = pPacket->getRackType();

    // Work on the parameters and the information pulled out of the packet
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Zone* pZone = pPC->getZone();
    Creature* pNPCBase = NULL;


    // NoSuch removed.
    pNPCBase = pZone->getCreature(NPCID);

    if (pNPCBase == NULL || pNPCBase->isNPC() == false) {
        GCNPCResponse gcNPCResponse;
        pPlayer->sendPacket(&gcNPCResponse);
        return;
    }

    NPC* pNPC = dynamic_cast<NPC*>(pNPCBase);

    if (type == SHOP_RACK_SPECIAL) {
        // Write the list of goods into the packet.
        GCShopList pkt;
        pkt.setNPCShopType(pNPC->getShopType());
        pkt.setObjectID(NPCID);
        pkt.setShopVersion(pNPC->getShopVersion(type));
        pkt.setShopType(type);

        for (BYTE i = 0; i < SHOP_RACK_INDEX_MAX; i++) {
            // Write each item's information.
            Item* pItem = pNPC->getShopItem(type, i);
            if (pItem != NULL)
                pkt.setShopItem(i, pItem);
        }

        pkt.setMarketCondBuy(pNPC->getMarketCondBuy());
        pkt.setMarketCondSell(pNPC->getTaxRatio(pPC));

        // Send the packet.
        pPlayer->sendPacket(&pkt);
    } else if (type == SHOP_RACK_MYSTERIOUS) {
        // Write the list of goods into the packet.
        GCShopListMysterious pkt;
        pkt.setObjectID(NPCID);
        pkt.setShopVersion(pNPC->getShopVersion(type));
        pkt.setShopType(type);

        for (BYTE i = 0; i < SHOP_RACK_INDEX_MAX; i++) {
            // Write each item's information.
            Item* pItem = pNPC->getShopItem(type, i);
            if (pItem != NULL)
                pkt.setShopItem(i, pItem);
        }

        pkt.setMarketCondBuy(pNPC->getMarketCondBuy());
        pkt.setMarketCondSell(pNPC->getTaxRatio(pPC));

        // Send the packet.
        pPlayer->sendPacket(&pkt);
    } else {
        throw ProtocolException("NORMAL shop item list not allowed!!!");
    }

#endif

    __END_DEBUG_EX
}
