//////////////////////////////////////////////////////////////////////////////
// Filename    : CGVerifyTimeHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGVerifyTime.h"

#ifdef __GAME_SERVER__
#include "Assert.h"
#include "GamePlayer.h"
#include "KernelContext.h"
#include "Ousters.h"
#include "Properties.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/SessionRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGVerifyTimeHandler::execute(CGVerifyTime* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    if (!pGamePlayer->verifySpeed()) {
        saveSpeedHackPlayer(pPlayer);
        throw DisconnectException("Speed hack detected");
    }

#endif

    __END_DEBUG_EX
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGVerifyTimeHandler::saveSpeedHackPlayer(Player* pPlayer) {
#ifdef __GAME_SERVER__
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    string ID = pPlayer->getID();
    string IP = pPlayer->getSocket()->getHost();
    string Name = "";

    if (pCreature != NULL) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Name = "Slayer:" + pSlayer->getName();
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            Name = "Vampire:" + pVampire->getName();
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            Name = "Ousters:" + pOusters->getName();
        }
    }

    filelog("SpeedHack.log", "ID[%s], Name[%s], IP[%s]", ID.c_str(), Name.c_str(), IP.c_str());

    /////////////////////////////////////////////////////////
    // Put the related information into the DB.
    /////////////////////////////////////////////////////////
    try {
        static WorldID_t WorldID = de::kernelContext().config().getPropertyInt("WorldID");
        static ServerGroupID_t ServerGroupID = de::kernelContext().config().getPropertyInt("ServerID");

        // The UPDATE and, when it changed no row, the INSERT IGNORE. A SQL
        // failure is END_DB's DatabaseError, which the catch below does not
        // match.
        defaultSessionRepository().recordSpeedHack(ID, IP, Name, (int)WorldID, (int)ServerGroupID);
    } catch (Throwable& t) {
        filelog("SpeedHackLogError.log", "%s", t.toString().c_str());
    }

#endif
}
