////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRecallSiege.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRecallSiege.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "CreatureUtil.h"
#include "EventTransport.h"
#include "GCGuildMemberList.h"
#include "GCModifyInformation.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SiegeManager.h"
#include "SiegeWar.h"
#include "StringPool.h"
#include "StringStream.h"
#include "WarSchedule.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionRecallSiege::read(PropertyBuffer& pb)

{
    try {
        m_ZoneID = pb.getPropertyInt("ZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionRecallSiege::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());


    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    Zone* pZone = getZoneByZoneID(m_ZoneID);
    Assert(pZone != NULL);

    WarScheduler* pWS = pZone->getWarScheduler();
    Assert(pWS != NULL);

    ZoneID_t siegeZoneID = SiegeManager::Instance().getSiegeZoneID(m_ZoneID);
    Assert(siegeZoneID != 0);


    if (!context().guilds().isGuildMaster(pPC->getGuildID(), pPC)) {
        GCSystemMessage gcSM;
        gcSM.setMessage("Only the guild master can apply.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    // Get the guild the PC belongs to.
    Guild* pGuild = context().guilds().getGuild(pPC->getGuildID());
    if (pGuild == NULL)
        return;

    GCGuildMemberList gcGuildMemberList;

    pGuild->makeMemberInfo(gcGuildMemberList);
    gcGuildMemberList.setType(1);

    pGamePlayer->sendPacket(&gcGuildMemberList);

    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRecallSiege::toString() const

{
    StringStream msg;
    msg << "ActionRecallSiege(" << ")";
    return msg.toString();
}
