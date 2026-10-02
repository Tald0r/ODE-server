//////////////////////////////////////////////////////////////////////////////
// Filename    : CLChangeServerHandler.cpp
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLChangeServer.h"

#ifdef __LOGIN_SERVER__
#include "Assert1.h"
#include "DatabaseError.h"
#include "GameServerGroupInfoManager.h"
#include "GameServerInfoManager.h"
#include "LCPCList.h"
#include "LoginCharacterList.h"
#include "LoginPlayer.h"
#include "OptionInfo.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a client asks for the list of PCs, the login server loads the PCs'
// information from the DB and sends it in an LCPCList packet.
//////////////////////////////////////////////////////////////////////////////
void CLChangeServerHandler::execute(CLChangeServer* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    ServerGroupID_t CurrentServerGroupID = pPacket->getServerGroupID();
    pLoginPlayer->setServerGroupID(CurrentServerGroupID);

    try {
        auto lcPCList = de::makeLoginCharacterList(pLoginPlayer->getWorldID(), pLoginPlayer->getID(),
                                                   defaultLoginCharacterRepository());
        pLoginPlayer->sendPacket(lcPCList.get());
        pLoginPlayer->setPlayerStatus(LPS_PC_MANAGEMENT);

        defaultLoginAccountRepository().setCurrentServerGroup((int)pPacket->getServerGroupID(), pLoginPlayer->getID());
    } catch (const DatabaseError& error) {
        // A SQL failure arrives as END_DB's DatabaseError carrying the line
        // it wrote to DBError.log; the reason travels with the disconnect.
        throw DisconnectException("CLChangeServerHandler : " + error.message());
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
