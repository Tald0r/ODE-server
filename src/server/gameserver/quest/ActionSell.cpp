////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSell.cpp
// Written By  :
// Description :
// Action where the NPC sells items to the player. In practice it
// starts the trade; all it does is send the player the NPC's
// current shop version as a packet.
////////////////////////////////////////////////////////////////////////////////

#include "ActionSell.h"

#include "GCNPCResponse.h"
#include "GCShopVersion.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "Script.h"
#include "Trigger.h"

////////////////////////////////////////////////////////////////////////////////
// The ActionSell action just sends the ShopVersion to the client.
// there are no parameters that need reading.
////////////////////////////////////////////////////////////////////////////////
void ActionSell::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionSell::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    GCShopVersion pkt;
    pkt.setObjectID(pNPC->getObjectID());
    pkt.setVersion(SHOP_RACK_NORMAL, pNPC->getShopVersion(SHOP_RACK_NORMAL));
    pkt.setVersion(SHOP_RACK_SPECIAL, pNPC->getShopVersion(SHOP_RACK_SPECIAL));
    pkt.setVersion(SHOP_RACK_MYSTERIOUS, pNPC->getShopVersion(SHOP_RACK_MYSTERIOUS));

    int taxratio = pNPC->getTaxRatio(pPC);

    if (taxratio != 100)
        pkt.setMarketCondSell(taxratio);
    else
        pkt.setMarketCondSell(pNPC->getMarketCondSell());

    pPlayer->sendPacket(&pkt);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSell::toString() const

{
    StringStream msg;
    msg << "ActionSell(" << ")";
    return msg.toString();
}
