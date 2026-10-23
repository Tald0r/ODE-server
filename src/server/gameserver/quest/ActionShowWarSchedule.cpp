////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionShowWarSchedule.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionShowWarSchedule.h"

#include "Creature.h"
#include "GCNPCResponse.h"
#include "GCWarScheduleList.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "NPC.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "WarScheduler.h"
#include "Zone.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionShowWarSchedule::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script id
        m_ZoneID = propertyBuffer.getPropertyInt("ZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionShowWarSchedule::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    GCWarScheduleList gcWarScheduleList;

    if (context().variables().isWarActive() && makeGCWarScheduleList(&gcWarScheduleList, m_ZoneID)) {
        pPC->getPlayer()->sendPacket(&gcWarScheduleList);
    } else {
        GCNPCResponse gcNPCResponse;
        gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
    }
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionShowWarSchedule::toString() const

{
    StringStream msg;
    msg << "ActionShowWarSchedule(" << ",ZoneID:" << (int)m_ZoneID << ")";

    return msg.toString();
}
