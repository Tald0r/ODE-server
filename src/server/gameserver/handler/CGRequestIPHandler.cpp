//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestIPHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestIP.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCRequestFailed.h"
#include "GCRequestedIP.h"
#include "GamePlayer.h"
#include "PCFinder.h"
#include "repository/SessionRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestIPHandler::execute(CGRequestIP* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        // Query the user IP from the UserIPInfo table.
        {
            DWORD ip = 0;
            DWORD port = 0;

            if (!defaultSessionRepository().loadUserIP(pPacket->getName(), ip, port)) {
                throw NoSuchElementException("No IP information for the requested ID.");
            } else {
                IP_t IP = ip;
                uint Port = port;

                GCRequestedIP gcRequestedIP;
                gcRequestedIP.setIP(IP);
                gcRequestedIP.setPort(Port);
                gcRequestedIP.setName(pPacket->getName().c_str());
                pPlayer->sendPacket(&gcRequestedIP);
            }
        }
    } catch (Throwable& t) {
        // When there is no such one..
        GCRequestFailed gcRequestFailed;
        gcRequestFailed.setCode(REQUEST_FAILED_IP);
        gcRequestFailed.setName(pPacket->getName());
        pPlayer->sendPacket(&gcRequestFailed);
    } catch (...) {
        GCRequestFailed gcRequestFailed;
        gcRequestFailed.setCode(REQUEST_FAILED_IP);
        gcRequestFailed.setName(pPacket->getName());
        pPlayer->sendPacket(&gcRequestFailed);
    }

#endif

    __END_DEBUG_EX
}
