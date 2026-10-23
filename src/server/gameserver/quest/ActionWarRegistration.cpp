////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWarRegistration.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWarRegistration.h"

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
#include "SystemAvailabilitiesManager.h"
#include "VariableManager.h"
#include "WarScheduler.h"
#include "Zone.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionWarRegistration::read(PropertyBuffer& propertyBuffer)

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
void ActionWarRegistration::execute(Creature* pCreature1, Creature* pCreature2)

{
    GuildManager& guilds = context().guilds();

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

    if (!guilds.isGuildMaster(guildID, pPC)) {
        gcNPCResponse.setCode(NPC_RESPONSE_NOT_GUILD_MASTER);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    Gold_t warRegistrationFee = context().variables().getVariable(WAR_REGISTRATION_FEE);
    if (pPC->getGold() < warRegistrationFee) {
        gcNPCResponse.setCode(NPC_RESPONSE_NOT_ENOUGH_MONEY);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    Zone* pZone = getZoneByZoneID(m_ZoneID);
    Assert(pZone != NULL);
    Assert(pZone->isCastle());

    WarScheduler* pWarScheduler = pZone->getWarScheduler();
    Assert(pWarScheduler != NULL);

    // Is this guild already holding a castle?
    if (guilds.hasCastle(guildID)) {
        gcNPCResponse.setCode(NPC_RESPONSE_ALREADY_HAS_CASTLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }


    CastleInfo* pCastleInfo = context().castleInfos().getCastleInfo(m_ZoneID);
    GuildID_t ownerGuildID = pCastleInfo->getGuildID();

    if (guildID == ownerGuildID) {
        gcNPCResponse.setCode(NPC_RESPONSE_ALREADY_HAS_CASTLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    // Has a war already been registered?
    if (guilds.hasWarSchedule(guildID)) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_ALREADY_REGISTERED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    // Is the war schedule full?
    if (!pWarScheduler->canAddWar(WAR_GUILD)) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_SCHEDULE_FULL);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    GuildWar* pWar = new GuildWar(m_ZoneID, guildID, War::WAR_STATE_WAIT);

    pWar->setRegistrationFee(warRegistrationFee);

    if (!pWarScheduler->addWar(pWar)) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_SCHEDULE_FULL);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

        SAFE_DELETE(pWar);
        return;
    }

    pPC->decreaseGoldEx(warRegistrationFee);

    GCModifyInformation gcMI;
    gcMI.addLongData(MODIFY_GOLD, pPC->getGold());
    pPC->getPlayer()->sendPacket(&gcMI);

    gcNPCResponse.setCode(NPC_RESPONSE_WAR_REGISTRATION_OK);
    pPC->getPlayer()->sendPacket(&gcNPCResponse);
    return;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWarRegistration::toString() const

{
    StringStream msg;
    msg << "ActionWarRegistration(" << ",ZoneID:" << (int)m_ZoneID << ")";

    return msg.toString();
}
