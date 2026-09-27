//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyTaxRatioHandler.cp	p
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGModifyTaxRatio.h"

#ifdef __GAME_SERVER__
#include "CastleInfoManager.h"
#include "GCModifyInformation.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGModifyTaxRatioHandler::execute(CGModifyTaxRatio* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    GuildID_t guildID = pPC->getGuildID();

    list<CastleInfo*> pCastleInfoList = de::gameContext().castleInfos().getGuildCastleInfos(guildID);
    if (pCastleInfoList.empty()) {
        GCNPCResponse fail;
        fail.setCode(NPC_RESPONSE_MODIFY_TAX_RATIO_FAIL);

        pGamePlayer->sendPacket(&fail);
        return;
    }

    bool bOwner = false;
    list<CastleInfo*>::iterator itr = pCastleInfoList.begin();
    for (; itr != pCastleInfoList.end(); itr++) {
        if ((*itr)->getZoneID() == pPC->getZoneID()) {
            bOwner = true;
            break;
        }
    }

    if (!de::gameContext().guilds().isGuildMaster(guildID, pPC) // not the guild master.
        || !bOwner                                              // not a castle this player's guild has taken.
        || pPacket->getRatio() > 10) {
        GCNPCResponse fail;
        fail.setCode(NPC_RESPONSE_MODIFY_TAX_RATIO_FAIL);

        pGamePlayer->sendPacket(&fail);
        return;
    }

    de::gameContext().castleInfos().setItemTaxRatio(pPC->getZone(), pPacket->getRatio() + 100);
    GCNPCResponse ok;
    ok.setCode(NPC_RESPONSE_MODIFY_TAX_RATIO_OK);
    pGamePlayer->sendPacket(&ok);

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
