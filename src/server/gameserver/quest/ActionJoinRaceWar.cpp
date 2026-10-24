////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionJoinRaceWar.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionJoinRaceWar.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "KernelContext.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "RaceWarLimiter.h"
#include "SystemAvailabilitiesManager.h"
#include "VariableManager.h"
#include "WarSystem.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionJoinRaceWar::read(PropertyBuffer& propertyBuffer)

{
    try {
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionJoinRaceWar::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_RACE_WAR);

    GCNPCResponse gcNPCResponse;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    static int isJoinRaceWar = de::kernelContext().config().getPropertyInt("JoinRaceWar");

    // Race war applications are turned off on this server.
    if (!isJoinRaceWar) {
        gcNPCResponse.setCode(NPC_RESPONSE_RACE_WAR_GO_FIRST_SERVER);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        return;
    }

    if (!context().variables().isWarActive() || context().warSystem().hasActiveRaceWar()) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_UNAVAILABLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    if (pCreature2->isFlag(Effect::EFFECT_CLASS_RACE_WAR_JOIN_TICKET)) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_ALREADY_REGISTERED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    if (!RaceWarLimiter::getInstance()->join(pPC)) {
        gcNPCResponse.setCode(NPC_RESPONSE_RACE_WAR_JOIN_FAILED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    gcNPCResponse.setCode(NPC_RESPONSE_RACE_WAR_JOIN_OK);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);
    return;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionJoinRaceWar::toString() const

{
    StringStream msg;
    msg << "ActionJoinRaceWar(" << ")";

    return msg.toString();
}
