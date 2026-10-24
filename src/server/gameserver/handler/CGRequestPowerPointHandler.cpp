//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestPowerPointHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestPowerPoint.h"

#ifdef __GAME_SERVER__
#include "Assert1.h"
#include "Creature.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "mofus/Mofus.h"

#ifdef __MOFUS__
#include "mofus/MPlayerManager.h"
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestPowerPointHandler::execute(CGRequestPowerPoint* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);


#ifdef __MOFUS__
    de::gameContext().mofusPlayers().addJob(pGamePlayer->getID(), pCreature->getName(), pPacket->getCellNum());
#endif

#endif

    __END_DEBUG_EX
}
