//----------------------------------------------------------------------
//
// Filename    : GSAddGuildHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Assert1.h"
#include "GSAddGuild.h"

#ifdef __SHARED_SERVER__

#include <limits>

#include "GameServerManager.h"
#include "Guild.h"
#include "GuildDecision.h"
#include "GuildManager.h"
#include "Properties.h"
#include "SGAddGuildMemberOK.h"
#include "SGAddGuildOK.h"
#include "SharedContext.h"

#endif

//----------------------------------------------------------------------
//
// GSAddGuildHandler::execute()
//
//----------------------------------------------------------------------
void GSAddGuildHandler::execute(GSAddGuild* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __SHARED_SERVER__

        Assert(pPacket != NULL);

    static_assert(kGuildRaceSlayer == Guild::GUILD_RACE_SLAYER);
    static_assert(kGuildRaceVampire == Guild::GUILD_RACE_VAMPIRE);
    static_assert(kGuildRaceOusters == Guild::GUILD_RACE_OUSTERS);

    AddGuildRequest request;
    request.guildRace = pPacket->getGuildRace();

    // A full counter founds nothing: the next id would wrap to zero.
    if (Guild::getMaxGuildID() == std::numeric_limits<GuildID_t>::max())
        return;

    // The id is claimed only once the founding is accepted.
    request.guildID = Guild::getMaxGuildID() + 1;

    request.maxSlayerZoneID = Guild::getMaxSlayerZoneID();
    request.maxVampireZoneID = Guild::getMaxVampireZoneID();
    request.maxOustersZoneID = Guild::getMaxOustersZoneID();

    Outcome<AddGuildAllocation, SharedGuildRejection> outcome = decideAddGuild(request);
    if (outcome.isRejected())
        return;

    const AddGuildAllocation& allocation = outcome.events();

    Guild::setMaxGuildID(allocation.guildID);

    if (request.guildRace == Guild::GUILD_RACE_SLAYER)
        Guild::setMaxSlayerZoneID(allocation.zoneID + 1);
    else if (request.guildRace == Guild::GUILD_RACE_VAMPIRE)
        Guild::setMaxVampireZoneID(allocation.zoneID + 1);
    else
        Guild::setMaxOustersZoneID(allocation.zoneID + 1);

    Guild* pGuild = new Guild();
    pGuild->setID(allocation.guildID);
    pGuild->setName(pPacket->getGuildName());
    pGuild->setRace(pPacket->getGuildRace());
    pGuild->setState(pPacket->getGuildState());
    pGuild->setServerGroupID(pPacket->getServerGroupID());
    pGuild->setZoneID(allocation.zoneID);
    pGuild->setMaster(pPacket->getGuildMaster());
    pGuild->setIntro(pPacket->getGuildIntro());

    pGuild->create();

    de::sharedContext().guilds().addGuild(pGuild);

    // The master is the guild's first member.
    GuildMember* pGuildMember = new GuildMember();
    pGuildMember->setGuildID(pGuild->getID());
    pGuildMember->setName(pGuild->getMaster());
    pGuildMember->setRank(GuildMember::GUILDMEMBER_RANK_MASTER);

    pGuildMember->create();

    pGuild->addMember(pGuildMember);

    SGAddGuildOK sgAddGuildOK;
    sgAddGuildOK.setGuildID(pGuild->getID());
    sgAddGuildOK.setGuildName(pGuild->getName());
    sgAddGuildOK.setGuildRace(pGuild->getRace());
    sgAddGuildOK.setGuildState(pGuild->getState());
    sgAddGuildOK.setServerGroupID(pGuild->getServerGroupID());
    sgAddGuildOK.setGuildZoneID(pGuild->getZoneID());
    sgAddGuildOK.setGuildMaster(pGuild->getMaster());
    sgAddGuildOK.setGuildIntro(pGuild->getIntro());

    de::sharedContext().gameServers().broadcast(&sgAddGuildOK);

    SGAddGuildMemberOK sgAddGuildMemberOK;
    sgAddGuildMemberOK.setGuildID(pGuildMember->getGuildID());
    sgAddGuildMemberOK.setName(pGuildMember->getName());
    sgAddGuildMemberOK.setGuildMemberRank(pGuildMember->getRank());
    sgAddGuildMemberOK.setServerGroupID(pPacket->getServerGroupID());

    de::sharedContext().gameServers().broadcast(&sgAddGuildMemberOK);

#endif

    __END_DEBUG_EX __END_CATCH
}
