//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUntransformHandler.cpp
// Written By  : reiot@ewestsoft.com , elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUntransform.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCRemoveEffect.h"
#include "GCUntransformFail.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUntransformHandler::execute(CGUntransform* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
    }

#endif

    __END_DEBUG_EX
}
