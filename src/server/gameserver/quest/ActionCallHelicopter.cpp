////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionCallHelicopter.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionCallHelicopter.h"

#include "Effect.h"
#include "GCNPCResponse.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "Slayer.h"

////////////////////////////////////////////////////////////////////////////////
// read from property buffer
////////////////////////////////////////////////////////////////////////////////
void ActionCallHelicopter::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionCallHelicopter::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    // Only a Slayer is a target of this action.
    if (!pCreature2->isSlayer())
        return;
    if (pCreature2->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pCreature2->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))
        return;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
    Assert(pSlayer != NULL);

    // Apply the effect to the Slayer.
    if (!pSlayer->isFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL))
        pSlayer->setFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL);

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse gcNPCResponse;
    gcNPCResponse.setCode(NPC_RESPONSE_INTERFACE_HELICOPTER);
    pPlayer->sendPacket(&gcNPCResponse);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionCallHelicopter::toString() const

{
    StringStream msg;
    msg << "ActionCallHelicopter(" << ")";
    return msg.toString();
}
