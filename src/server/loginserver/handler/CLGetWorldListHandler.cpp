//////////////////////////////////////////////////////////////////////////////
// Filename    : CLGetWorldListHandler.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLGetWorldList.h"

#ifdef __LOGIN_SERVER__
#include <vector>

#include "Assert1.h"
#include "GameWorldInfoManager.h"
#include "LCWorldList.h"
#include "LoginPlayer.h"
#include "ServerContext.h"
#include "WorldInfo.h"
#include "repository/LoginAccountRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
// When a client asks for the list of servers, the login server loads the
// servers' information from the DB and sends it in an LCWorldList packet.
//////////////////////////////////////////////////////////////////////////////
void CLGetWorldListHandler::execute(CLGetWorldList* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __LOGIN_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    LoginPlayer* pLoginPlayer = dynamic_cast<LoginPlayer*>(pPlayer);

    try {
        int Num = de::serverContext().worldInfos().getSize();


        // Worlds are numbered from 1, so the table has one unused slot at 0.
        std::vector<WorldInfo*> aWorldInfo(Num + 1, nullptr);

        for (int i = 1; i < Num + 1; i++) {
            WorldInfo* pWorldInfo = new WorldInfo();
            const GameWorldInfo* pGameWorldInfo = de::serverContext().worldInfos().getGameWorldInfo(i);
            pWorldInfo->setID(pGameWorldInfo->getID());
            pWorldInfo->setName(pGameWorldInfo->getName());

            // by bezz. 2002.12.20
            pWorldInfo->setStat(pGameWorldInfo->getStatus());

            aWorldInfo[i] = pWorldInfo;
        }

        LCWorldList lcWorldList;

        int currentWorldID = 0;
        if (defaultLoginAccountRepository().loadCurrentWorld(pLoginPlayer->getID(), currentWorldID)) {
            lcWorldList.setCurrentWorldID(currentWorldID);
        }

        for (int k = 1; k < Num + 1; k++) {
            lcWorldList.addListElement(aWorldInfo[k]);
        }

        pLoginPlayer->sendPacket(&lcWorldList);
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
