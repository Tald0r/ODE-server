////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionShowTaxBalance.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionShowTaxBalance.h"

#include "CastleInfoManager.h"
#include "Creature.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "NPC.h"
#include "PlayerCreature.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionShowTaxBalance::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionShowTaxBalance::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    bool bSuccess = true;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    GuildID_t guildID = pPC->getGuildID();
    GCNPCResponse deny;

    Guild* pGuild = context().guilds().getGuild(guildID);
    if (bSuccess && pGuild == NULL) {
        // No guild.
        bSuccess = false;
        deny.setCode(NPC_RESPONSE_NO_GUILD);
    }

    if (bSuccess && pGuild->getMaster() != pPC->getName()) {
        // Not the guild master.
        bSuccess = false;
        deny.setCode(NPC_RESPONSE_NOT_GUILD_MASTER);
    }

    // The player is the guild master.
    list<CastleInfo*> pCastleInfoList = context().castleInfos().getGuildCastleInfos(guildID);
    if (bSuccess && pCastleInfoList.empty()) {
        // The guild owns no castle.
        bSuccess = false;
        deny.setCode(NPC_RESPONSE_HAS_NO_CASTLE);
    }

    list<CastleInfo*>::iterator itr = pCastleInfoList.begin();
    CastleInfo* pCastleInfo = NULL;

    for (; itr != pCastleInfoList.end(); itr++) {
        if ((*itr)->getZoneID() == pCreature1->getZoneID()) {
            pCastleInfo = (*itr);
            break;
        }
    }

    if (bSuccess && pCastleInfo == NULL) {
        bSuccess = false;
        deny.setCode(NPC_RESPONSE_NOT_YOUR_CASTLE);
    }

    if (bSuccess) {
        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_SHOW_TAX_BALANCE);
        response.setParameter((uint)pCastleInfo->getTaxBalance());
        pPlayer->sendPacket(&response);
    } else {
        pPlayer->sendPacket(&deny);
    }

    GCNPCResponse quit;
    quit.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&quit);
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionShowTaxBalance::toString() const

{
    StringStream msg;
    msg << "ActionShowTaxBalance(" << ")";

    return msg.toString();
}
