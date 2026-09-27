
//////////////////////////////////////////////////////////////////////
//
// Filename    : CGAuthKeyHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "CGAuthKey.h"

#ifdef __GAME_SERVER__
#include "EventKick.h"
#include "GCSystemMessage.h"
#include "GamePlayer.h"
#endif

//////////////////////////////////////////////////////////////////////
//
// Method run when the client receives a message from the server.
//
//////////////////////////////////////////////////////////////////////
void CGAuthKeyHandler::execute(CGAuthKey* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);


#endif

    __END_CATCH
}
