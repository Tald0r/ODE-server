//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectBloodBibleHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectBloodBible.h"

#ifdef __GAME_SERVER__
#include <algorithm>

#include "BloodBibleBonus.h"
#include "BloodBibleBonusManager.h"
#include "BloodBibleSignInfo.h"
#include "GCBloodBibleSignInfo.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PlayerCreature.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectBloodBibleHandler::execute(CGSelectBloodBible* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    BloodBibleBonus* pBonus = NULL;

    try {
        pBonus = de::gameContext().bloodBibleBonuses().getBloodBibleBonus(pPacket->getBloodBibleID());
        if (pBonus == NULL)
            return;
    } catch (NoSuchElementException& e) {
        return;
    }

    if (pPC->getRace() != pBonus->getRace()) {
        GCSystemMessage gcSM;
        gcSM.setMessage("You cannot use this Blood Bible.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    BloodBibleSignInfo* pInfo = pPC->getBloodBibleSign();
    if (pInfo->getOpenNum() <= pInfo->getList().size()) {
        GCSystemMessage gcSM;
        gcSM.setMessage("No free slot.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    if (find(pInfo->getList().begin(), pInfo->getList().end(), pPacket->getBloodBibleID()) != pInfo->getList().end()) {
        GCSystemMessage gcSM;
        gcSM.setMessage("This Blood Bible is already equipped.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    GCSystemMessage gcSM;
    gcSM.setMessage("Blood Bible equipped.");
    pGamePlayer->sendPacket(&gcSM);

    pInfo->getList().push_back(pPacket->getBloodBibleID());
    GCBloodBibleSignInfo gcInfo;
    gcInfo.setSignInfo(pInfo);
    pGamePlayer->sendPacket(&gcInfo);

    pPC->initAllStatAndSend();

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
