////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionDestroyGuild.cpp
// Written By  :
// Description :
// Makes the client open the guild disband window through the NPC.
////////////////////////////////////////////////////////////////////////////////

#include "ActionDestroyGuild.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "SystemAvailabilitiesManager.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionDestroyGuild::read(PropertyBuffer& propertyBuffer)

{
    // Opening the guild disband window takes no arguments, so nothing is read here.
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionDestroyGuild::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_GUILD);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse okpkt;
    okpkt.setCode(NPC_RESPONSE_INTERFACE_DESTROY_GUILD);
    pPlayer->sendPacket(&okpkt);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionDestroyGuild::toString() const

{
    StringStream msg;
    msg << "ActionDestroyGuild(" << ")";
    return msg.toString();
}
