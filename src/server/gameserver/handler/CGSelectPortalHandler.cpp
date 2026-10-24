//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSelectPortalHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSelectPortal.h"

#ifdef __GAME_SERVER__
#include "GamePlayer.h"
#include "Portal.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSelectPortalHandler::execute(CGSelectPortal* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        // Just return if the game player's state is not normal.
        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        Creature* pCreature = pGamePlayer->getCreature();
        Zone* pZone = pCreature->getZone();

        // Return if the creature's coordinates are invalid.
        if (!isValidZoneCoord(pZone, pCreature->getX(), pCreature->getY()))
            return;

        Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());

        // When a portal is there and the creature is a PC.. (monsters and NPCs do not move through portals.)
        if (rTile.hasPortal()) {
            Portal* pPortal = rTile.getPortal();

            if (pPortal->getPortalClass() == PORTAL_CLASS_MULTI) {
                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                    if (!pSlayer->hasRideMotorcycle() && (pPortal->getObjectType() == PORTAL_SLAYER)) {
                        pPortal->activate(pCreature, pPacket->getZoneID());
                    }
                } else {
                    if (pPortal->getObjectType() == PORTAL_NORMAL || pPortal->getObjectType() == PORTAL_SLAYER) {
                        pPortal->activate(pCreature, pPacket->getZoneID());
                    }
                }
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
