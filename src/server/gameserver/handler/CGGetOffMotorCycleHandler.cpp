//////////////////////////////////////////////////////////////////////////////
// Filename    : CGGetOffMotorCycleHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGGetOffMotorCycle.h"

#ifdef __GAME_SERVER__
#include "GCGetOffMotorCycle.h"
#include "GCGetOffMotorCycleFailed.h"
#include "GCGetOffMotorCycleOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Slayer.h"
#include "ctf/FlagManager.h"
#include "item/Motorcycle.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGGetOffMotorCycleHandler::execute(CGGetOffMotorCycle* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    // Check that the player's state is normal.
    if (pGamePlayer->getPlayerStatus() != GPS_NORMAL) {
        GCGetOffMotorCycleFailed _GCGetOffMotorCycleFailed;
        pPlayer->sendPacket(&_GCGetOffMotorCycleFailed);
        return;
    }

    Creature* pCreature = pGamePlayer->getCreature();

    // Check that it is a Slayer.
    if (!pCreature->isSlayer()) {
        GCGetOffMotorCycleFailed _GCGetOffMotorCycleFailed;
        pPlayer->sendPacket(&_GCGetOffMotorCycleFailed);
        return;
    }

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Motorcycle* pMotorcycle = pSlayer->getMotorcycle();
    bool bSuccess = false;
    Zone* pZone = pSlayer->getZone();

    // If riding a motorcycle...
    if (pMotorcycle != NULL) {
        ZoneCoord_t x = pSlayer->getX();
        ZoneCoord_t y = pSlayer->getY();

        // Outside an ordinary area (0 is the ordinary one), only a GM can get off.
        if (pZone->getZoneLevel(x, y) & SAFE_ZONE) {
            if (pSlayer->isGOD() || pSlayer->isDM())
                bSuccess = true;
        } else if (!de::gameContext().flags().isInPoleField(ZONE_COORD(pZone->getZoneID(), x, y))) {
            bSuccess = true;
        }
    }

    if (bSuccess) {
        pSlayer->getOffMotorcycle();

        GCGetOffMotorCycle _GCGetOffMotorCycle;
        GCGetOffMotorCycleOK _GCGetOffMotorCycleOK;

        _GCGetOffMotorCycle.setObjectID(pSlayer->getObjectID());

        pGamePlayer->sendPacket(&_GCGetOffMotorCycleOK);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCGetOffMotorCycle, pSlayer);
    } else {
        GCGetOffMotorCycleFailed _GCGetOffMotorCycleFailed;
        pPlayer->sendPacket(&_GCGetOffMotorCycleFailed);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
