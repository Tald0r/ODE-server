////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRegisterSiege.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionRegisterSiege.h"

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
#include "SiegeRegistrationDecision.h"
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
void ActionRegisterSiege::read(PropertyBuffer& propertyBuffer)

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
void ActionRegisterSiege::execute(Creature* pCreature1, Creature* pCreature2)

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


    CastleInfo* pCastleInfo = context().castleInfos().getCastleInfo(m_ZoneID);
    GuildID_t ownerGuildID = pCastleInfo->getGuildID();

    if (guildID == ownerGuildID) {
        gcNPCResponse.setCode(NPC_RESPONSE_ALREADY_HAS_CASTLE);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    // Has a war already been registered?
    if (context().guilds().hasWarSchedule(guildID)) {
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_ALREADY_REGISTERED);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);
        return;
    }

    // Is the war schedule full?

    // The castle's next war. A scheduler holds only its own castle's wars,
    // so this is the war the registration either joins or is turned away by.
    // Both castle war classes report WAR_GUILD, so the war is read through
    // the schedule and only the join itself needs the siege's challenger
    // list: a guild war waiting here is applied for by one guild alone and
    // has no slot to join.
    WarSchedule* pNextWarSchedule = dynamic_cast<WarSchedule*>(pWarScheduler->getRecentSchedule());
    War* pNextCastleWar = (pNextWarSchedule != NULL) ? pNextWarSchedule->getWar() : NULL;
    SiegeWar* pNextWar = dynamic_cast<SiegeWar*>(pNextCastleWar);

    SiegeRegistrationState state;
    state.hasScheduledWar = (pNextCastleWar != NULL);
    state.isSiege = (pNextWar != NULL);
    state.challengerCount = (pNextWar != NULL) ? pNextWar->getChallengerGuildCount() : 0;

    switch (decideSiegeRegistration(state)) {
    case SIEGE_REGISTRATION_CREATE: {
        pNextWar = new SiegeWar(m_ZoneID, War::WAR_STATE_WAIT);
        pNextWar->addRegistrationFee(warRegistrationFee);
        pNextWar->addChallengerGuild(guildID);

        if (!pWarScheduler->addWar(pNextWar)) {
            gcNPCResponse.setCode(NPC_RESPONSE_WAR_SCHEDULE_FULL);
            pPC->getPlayer()->sendPacket(&gcNPCResponse);

            SAFE_DELETE(pNextWar);
            return;
        }
        break;
    }

    case SIEGE_REGISTRATION_JOIN:
        pNextWar->addRegistrationFee(warRegistrationFee);
        pNextWar->addChallengerGuild(guildID);
        pNextWarSchedule->save();
        break;

    case SIEGE_REGISTRATION_FULL:
        gcNPCResponse.setCode(NPC_RESPONSE_WAR_SCHEDULE_FULL);
        pPC->getPlayer()->sendPacket(&gcNPCResponse);

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
string ActionRegisterSiege::toString() const

{
    StringStream msg;
    msg << "ActionRegisterSiege(" << ",ZoneID:" << (int)m_ZoneID << ")";

    return msg.toString();
}
