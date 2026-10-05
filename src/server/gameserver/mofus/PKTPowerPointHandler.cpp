/////////////////////////////////////////////////////////////////////////////
// Filename : PKTPowerPointHandler.cpp
// Desc		:
/////////////////////////////////////////////////////////////////////////////

// include files
#include "PKTPowerPointHandler.h"

#include "Assert.h"
#include "KernelContext.h"
#include "MJob.h"
#include "MPacketDiagnostics.h"
#include "MPlayer.h"
#include "Mofus.h"
#include "PKTError.h"
#include "PKTPowerPoint.h"
#include "PKTSError.h"
#include "Properties.h"

// run function
void PKTPowerPointHandler::execute(MPlayer* pPlayer, MPacket* pPacket) {
    PKTPowerPoint* pPowerPoint = dynamic_cast<PKTPowerPoint*>(pPacket);
    Assert(pPowerPoint != NULL);

    cout << "--------------------------------------------------" << endl;
    cout << "RECV [" << pPlayer->getJob()->getName() << "] PowerPoint (name:" << pPowerPoint->sCharName
         << ",point:" << pPowerPoint->nPowerPoint << ")" << endl;
    cout << "--------------------------------------------------" << endl;

    filelog(MOFUS_LOG_FILE, "RECV [%s] PowerPoint (name:%s,point:%d)", pPlayer->getJob()->getName().c_str(),
            pPowerPoint->sCharName, pPowerPoint->nPowerPoint);
    de::logMofusPacketMetadata(MOFUS_PACKET_FILE, "receive", *pPacket);

    //////////////////////////////////////////////////////////////////////
    // Check the packet that was received.
    //////////////////////////////////////////////////////////////////////
    static int MofusServerCode = de::kernelContext().config().getPropertyInt("MofusServerCode");
    // Check the game code
    bool bCheckGameCode = (pPowerPoint->getGameCode() == 1);
    // Check the server code
    bool bCheckGameServerCode = (pPowerPoint->getGameServerCode() == MofusServerCode);
    // Check the character name
    bool bCheckCharacterName = (strcasecmp(pPlayer->getJob()->getName().c_str(), pPowerPoint->getCharacterName()) == 0);

    if (!bCheckGameCode || !bCheckGameServerCode || !bCheckCharacterName) {
        cout << "--------------------------------------------------" << endl;
        cout << "ERROR CHECK (name:" << pPlayer->getJob()->getName() << ",mofusname:" << pPowerPoint->getCharacterName()
             << ",gameservercode:" << MofusServerCode << ",mofusgameservercode:" << pPowerPoint->getGameServerCode()
             << ",gamecode:" << pPowerPoint->getGameCode() << ")" << endl;
        cout << "--------------------------------------------------" << endl;

        filelog(MOFUS_LOG_FILE, "ERROR (name:%s,mofusname:%s,gameservercode:%d,mofusgameservercode:%d)",
                pPlayer->getJob()->getName().c_str(), pPowerPoint->getCharacterName(), MofusServerCode,
                pPowerPoint->getGameServerCode());

        // The matching information check failed
        // Tell the PowerJjang server that the check failed.
        pPlayer->sendSError(MSERR_MATCH);

        // Report it to the user as a matching information error.
        pPlayer->setErrorCode(MERR_MATCHING);

        // work finished
        pPlayer->setEnd();

        return;
    }
    //////////////////////////////////////////////////////////////////////

    // Save the power points
    // The maximum PowerJjang points applied per transfer.
    // If the PowerJjang server has 60 points stored, all 60 are received, and
    // the maximum applied per transfer is 40,
    // only 40 are applied and the other 20 are thrown away.
    // So the user loses all 60 points on the PowerJjang server
    // while only 40 accumulate in the DarkEden DB.
    static int MaxPowerPoint = 40;

    // Apply the maximum PowerJjang points per transfer
    int savepowerpoint = min(pPowerPoint->getPowerPoint(), MaxPowerPoint);

    // Accumulate the power points that were fetched into the DB
    savePowerPoint(pPlayer->getJob()->getName(), savepowerpoint);

    // file log
    filelog(MOFUS_LOG_FILE, "SAVE PowerPoint (name:%s,savepoint:%d,recvpoint:%d)", pPlayer->getJob()->getName().c_str(),
            savepowerpoint, pPowerPoint->getPowerPoint());

    logPowerPoint(pPlayer->getJob()->getName(), pPowerPoint->getPowerPoint(), savepowerpoint);

    // Accumulate the power points that were received
    pPlayer->addPowerPoint(savepowerpoint);

    // Tell the PowerJjang server that it was handled.
    if (pPowerPoint->isContinue()) {
        // There is more work.
        pPlayer->sendReceiveOK();
    } else {
        pPlayer->sendResult();
        pPlayer->sendLogout();

        // The work is done.
        pPlayer->setEnd();
    }
}
