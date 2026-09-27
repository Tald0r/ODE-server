//////////////////////////////////////////////////////////////////////////////
// Filename    : CGLogoutHandler.cc
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGLogout.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "Creature.h"
#include "GCSystemMessage.h"
#include "GLIncomingConnection.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "IncomingPlayerManager.h"
#include "Inventory.h"
#include "KernelContext.h"
#include "LoginServerManager.h"
#include "Ousters.h"
#include "PKZoneInfoManager.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// When the client sends a CGLogout packet, the game server deletes the creature from
// the zone, saves the creature and item information to the DB, and closes the connection.
//////////////////////////////////////////////////////////////////////////////
void CGLogoutHandler::execute(CGLogout* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPlayer != NULL);

    // Under the new login structure, a Logout has to leave into the waiting state.
    // On a Logout packet the player is sent to the IncomingPlayerManager.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);


    Creature* pCreature = pGamePlayer->getCreature();

    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();

    Assert(pZone != NULL);


    // Log that I am leaving.
    pGamePlayer->logLoginoutDateTime();

    try {
        // On logout, drop the relic and the blood bible fragments.

        if (pCreature->isPLAYER() && de::gameContext().pkZoneInfos().isPKZone(pCreature->getZoneID())) {
            de::gameContext().pkZoneInfos().leavePKZone(pCreature->getZoneID());
        }

        if (de::kernelContext().config().hasKey("Hardcore") &&
            de::kernelContext().config().getPropertyInt("Hardcore") != 0 && pPacket == NULL) {
        } else {
            // Save the creature's information.
            pCreature->save();

            if (pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                pSlayer->tinysave("LastPlayDate=now()");
            } else if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                pVampire->tinysave("LastPlayDate=now()");
            } else if (pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                pOusters->tinysave("LastPlayDate=now()");
            }

            //////////////////////////////////////////////////////////////
            // A player in the COMA state (currently dead) is moved automatically
            // to the revival position on logout.
            // This keeps repeated Login/Logout from replacing the revival skill
            //
            // The Creature's information is updated to the DB first, then updated again.
            //////////////////////////////////////////////////////////////

            // Logging out after using Eternity once sends one to the revival position.
            if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY)) {
                ZoneID_t ZoneID = 0;
                ZoneCoord_t ZoneX = 0;
                ZoneCoord_t ZoneY = 0;
                ZONE_COORD ResurrectCoord;

                if (pCreature->isPC()) {
                    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

                    de::gameContext().resurrectLocations().getPosition(pPC, ResurrectCoord);

                    ZoneID = ResurrectCoord.id;
                    ZoneX = ResurrectCoord.x;
                    ZoneY = ResurrectCoord.y;

                    char pField[80];
                    snprintf(pField, sizeof(pField), "ZoneID=%d, XCoord=%d, YCoord=%d, CurrentHP=HP", ZoneID, ZoneX,
                             ZoneY);

                    if (pPC->isSlayer()) {
                        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                        pSlayer->tinysave(pField);
                    } else if (pPC->isVampire()) {
                        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
                        pVampire->tinysave(pField);
                    } else if (pPC->isOusters()) {
                        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                        pOusters->tinysave(pField);
                    }
                }
            }
        }


        //
        // Now delete the PC from the zone.
        //
        // *CAUTION*
        //
        // pCreature's coordinates must match the coordinates of the tile it really sits on.
        // So the coordinates have to be set properly before calling this method.
        //
        pZone->deleteCreature(pCreature, pCreature->getX(), pCreature->getY());


        // Delete the player from the zone group's ZPM.
        // This runs inside ZonePlayerManager's ProcessCommand, so it must be deleted NoBlocked.
        pZone->getZoneGroup()->getZonePlayerManager()->deletePlayer(pGamePlayer->getSocket()->getSOCKET());

        // Move the player to the IPM.

        // With the Core structure changed, to act independently of the thread and handle it all at once later,
        // it goes into the OutList.
        pZone->getZoneGroup()->getZonePlayerManager()->pushOutPlayer(pGamePlayer);
    } catch (NoSuchElementException& nsee) {
        throw DisconnectException();
    }

    // Send GLIncomingConnection to the login server.
    // PlayerName and ClientIP are sent along with it.
    // add by zdj
    GLIncomingConnection glIncomingConnection;
    glIncomingConnection.setPlayerID(pGamePlayer->getID());
    glIncomingConnection.setClientIP(pGamePlayer->getSocket()->getHost());

    de::gameContext().loginServer().sendPacket(de::kernelContext().config().getProperty("LoginServerIP"), 9999,
                                               &glIncomingConnection);

    pGamePlayer->setPlayerStatus(GPS_AFTER_SENDING_GL_INCOMING_CONNECTION);

#endif

    __END_DEBUG_EX __END_CATCH
}
