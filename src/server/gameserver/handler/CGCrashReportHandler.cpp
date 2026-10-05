//////////////////////////////////////////////////////////////////////////////
// Filename    : CGCrashReportHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGCrashReport.h"

#ifdef __GAME_SERVER__
#include "GamePlayer.h"
#include "PacketDiagnostics.h"
#include "Slayer.h"
#include "repository/SessionRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGCrashReportHandler::execute(CGCrashReport* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

    Creature* pCreature = pGamePlayer->getCreature();

    try {
        defaultSessionRepository().insertCrashReport(
            pGamePlayer->getID(), pCreature->getName(), pPacket->getExecutableTime(), pPacket->getVersion(),
            pPacket->getAddress(), pPacket->getMessage(), pPacket->getOS(), pPacket->getCallStack());
        // Ignore anyone who sends something odd
    } catch (...) {
        de::logPacketMetadata("CrashReport.log", "crash report storage failed", *pPacket);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
