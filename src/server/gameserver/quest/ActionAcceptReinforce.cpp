////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAcceptReinforce.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAcceptReinforce.h"

#include "CastleInfoManager.h"
#include "Creature.h"
#include "GCModifyInformation.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "GuildWar.h"
#include "NPC.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "SiegeWar.h"
#include "SystemAvailabilitiesManager.h"
#include "VariableManager.h"
#include "WarSchedule.h"
#include "WarScheduler.h"
#include "Zone.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAcceptReinforce::read(PropertyBuffer& propertyBuffer)

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
void ActionAcceptReinforce::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    SYSTEM_RETURN_IF_NOT(SYSTEM_GUILD_WAR);

    GCNPCResponse gcNPCResponse;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    GuildID_t guildID = pPC->getGuildID();

    if (!context().variables().isWarActive() || !context().variables().isActiveGuildWar()) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_UNAVAILABLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    if (!context().guilds().isGuildMaster(guildID, pPC)) {
        gcNPCResponse.setCode(NPC_RESPONSE_NOT_GUILD_MASTER);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    Zone* pZone = getZoneByZoneID(m_ZoneID);
    Assert(pZone != NULL);
    Assert(pZone->isCastle());

    WarScheduler* pWarScheduler = pZone->getWarScheduler();
    Assert(pWarScheduler != NULL);

    CastleInfo* pCastleInfo = context().castleInfos().getCastleInfo(m_ZoneID);
    GuildID_t ownerGuildID = pCastleInfo->getGuildID();

    if (guildID != ownerGuildID) {
        gcNPCResponse.setCode(NPC_RESPONSE_NOT_YOUR_CASTLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    Schedule* pNextSchedule = pWarScheduler->getRecentSchedule();

    Work* pNextWork = NULL;
    if (pNextSchedule != NULL)
        pNextWork = pNextSchedule->getWork();

    SiegeWar* pNextWar = dynamic_cast<SiegeWar*>(pNextWork);

    if (pNextWar == NULL || pNextWar->recentReinforceCandidate() == 0) {
        gcNPCResponse.setCode(NPC_RESPONSE_NO_WAR_REGISTERED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        return;
    } else if (pNextWar->getReinforceGuildID() == 0) {
        WarSchedule* pNextWarSchedule = dynamic_cast<WarSchedule*>(pNextSchedule);
        Assert(pNextWarSchedule != NULL);

        if (!pNextWar->acceptReinforce()) {
            gcNPCResponse.setCode(NPC_RESPONSE_CANNOT_ACCEPT);
            pPC->getPlayer()->sendPacket(&gcNPCResponse);

            return;
        }
    } else {
        gcNPCResponse.setCode(NPC_RESPONSE_ALREADY_REINFORCE_ACCEPTED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        return;
    }

    gcNPCResponse.setCode(NPC_RESPONSE_ACCEPT_OK);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);

    gcNPCResponse.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);

    return;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAcceptReinforce::toString() const

{
    StringStream msg;
    msg << "ActionAcceptReinforce(" << ",ZoneID:" << (int)m_ZoneID << ")";

    return msg.toString();
}
