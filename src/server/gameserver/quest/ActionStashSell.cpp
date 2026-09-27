////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionStashSell.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionStashSell.h"

#include "Creature.h"
#include "GCStashSell.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "Vampire.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionStashSell::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY
    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionStashSell::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Price_t price = 0;
    BYTE curStashNum;
    GCStashSell pkt;

    if (pCreature2->isSlayer()) {
        // The number of stashes held decides the price of the next stash.
        curStashNum = dynamic_cast<Slayer*>(pCreature2)->getStashNum();

        if (curStashNum < STASH_RACK_MAX) {
            price = context().prices().getStashPrice(curStashNum + 1);
        } else
            price = 0;
    } else if (pCreature2->isVampire()) {
        // The number of stashes held decides the price of the next stash.
        curStashNum = dynamic_cast<Vampire*>(pCreature2)->getStashNum();

        if (curStashNum < STASH_RACK_MAX) {
            price = context().prices().getStashPrice(curStashNum + 1);
        } else
            price = 0;
    } else if (pCreature2->isOusters()) {
        // The number of stashes held decides the price of the next stash.
        curStashNum = dynamic_cast<Ousters*>(pCreature2)->getStashNum();

        if (curStashNum < STASH_RACK_MAX) {
            price = context().prices().getStashPrice(curStashNum + 1);
        } else
            price = 0;
    }

    // Set the value in the packet.
    pkt.setPrice(price);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);
    pPlayer->sendPacket(&pkt);

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionStashSell::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "ActionStashSell(" << ")";

    return msg.toString();

    __END_CATCH
}
