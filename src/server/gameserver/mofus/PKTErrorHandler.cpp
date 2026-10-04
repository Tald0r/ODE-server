/////////////////////////////////////////////////////////////////////////////
// Filename : PKTErrorHandler.cpp
// Desc		:
/////////////////////////////////////////////////////////////////////////////

// include files
#include "PKTErrorHandler.h"

#include "Assert.h"
#include "MJob.h"
#include "MPacketDiagnostics.h"
#include "MPlayer.h"
#include "Mofus.h"
#include "PKTError.h"


// run function
void PKTErrorHandler::execute(MPlayer* pPlayer, MPacket* pPacket) {
    PKTError* pError = dynamic_cast<PKTError*>(pPacket);
    Assert(pError != NULL);

    cout << "--------------------------------------------------" << endl;
    cout << "RECV [" << pPlayer->getJob()->getName() << "] Error : " << pError->nError << endl;
    cout << "--------------------------------------------------" << endl;

    filelog(MOFUS_LOG_FILE, "RECV [%s] Error : %d", pPlayer->getJob()->getName().c_str(), pError->nError);
    de::logMofusPacketMetadata(MOFUS_PACKET_FILE, "receive", *pPacket);

    // Set the error code
    pPlayer->setErrorCode(pError->getErrorCode());

    // Send the logout
    pPlayer->sendLogout();

    // work finished
    pPlayer->setEnd();
}
