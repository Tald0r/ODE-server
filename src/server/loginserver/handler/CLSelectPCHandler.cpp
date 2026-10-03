//////////////////////////////////////////////////////////////////////////////
// Filename    : CLSelectPCHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLSelectPC.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "GameServerManager.h"
#include "KernelContext.h"
#include "LGIncomingConnection.h"
#include "LoginCharacterSelection.h"
#include "LoginCharacterTopology.h"
#include "LoginContext.h"
#include "LoginIncomingRequest.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"

#endif

//////////////////////////////////////////////////////////////////////////////
// CLSelectPCHandler::execute()
//
// Loads the PC named by the packet, checks that the account may bring it
// into the game, and tells the game server that runs its zone to expect
// the incoming connection.
//////////////////////////////////////////////////////////////////////////////
void CLSelectPCHandler::execute(CLSelectPC* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    Properties& config = de::kernelContext().config();

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    LoginCharacterTopology topology(de::serverContext().serverInfos(), de::loginContext().zoneInfos(),
                                    de::loginContext().zoneGroupInfos());
    (void)de::selectLoginCharacter(
        *pLoginPlayer, *pPacket, topology,
        {de::serverContext().serverInfos(), config, defaultLoginAccountRepository(), defaultLoginCharacterRepository(),
         [](const std::string& host, uint port, const LGIncomingConnection& packet) {
             de::loginContext().gameServers().sendPacket(host, port, &packet);
         },
         [](const std::string& host, uint port) { cout << "gameserver ip: " << host << ", port: " << port << endl; }},
        de::defaultLoginCharacterSelectionActions());

#endif

    __END_DEBUG_EX __END_CATCH
}
