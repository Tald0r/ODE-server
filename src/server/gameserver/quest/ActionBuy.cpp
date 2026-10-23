////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionBuy.cpp
// Written By  :
// Description :
// Action where the NPC buys items from the player. It acknowledges the
// request with an NPC response, then sends the NPC's current buy and
// sell market conditions.
////////////////////////////////////////////////////////////////////////////////

#include "ActionBuy.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GCShopMarketCondition.h"
#include "GamePlayer.h"
#include "NPC.h"

////////////////////////////////////////////////////////////////////////////////
// The ActionBuy action takes no parameters, so there is nothing to read
// here.
//
// If a shop that buys only certain kinds of item is wanted later,
// a property can be added and read here. The NPC part would need
// changing too.
////////////////////////////////////////////////////////////////////////////////
void ActionBuy::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionBuy::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    // First tell the client that the answer was received.
    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    // Report the current market conditions.
    GCShopMarketCondition pkt;
    pkt.setObjectID(pNPC->getObjectID());
    pkt.setMarketCondBuy(pNPC->getMarketCondBuy());
    pkt.setMarketCondSell(pNPC->getMarketCondSell());
    pPlayer->sendPacket(&pkt);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionBuy::toString() const

{
    StringStream msg;
    msg << "ActionBuy(" << ")";
    return msg.toString();
}
