/////////////////////////////////////////////////////////////////////////////
// Filename : PKTConnectAcceptHandler.cpp
// Desc		:
/////////////////////////////////////////////////////////////////////////////

// include files
#include "PKTConnectAcceptHandler.h"

#include "MJob.h"
#include "MPacketDiagnostics.h"
#include "MPlayer.h"
#include "Mofus.h"


// run function
void PKTConnectAcceptHandler::execute(MPlayer* pPlayer, MPacket* pPacket) {
    cout << "--------------------------------------------------" << endl;
    cout << "RECV [" << pPlayer->getJob()->getName() << "] ConnectAccept" << endl;
    cout << "--------------------------------------------------" << endl;

    filelog(MOFUS_LOG_FILE, "RECV [%s] ConnectAccept", pPlayer->getJob()->getName().c_str());
    de::logMofusPacketMetadata(MOFUS_PACKET_FILE, "receive", *pPacket);

    // Ask for the user info. Fetch the power points
    pPlayer->sendUserInfo();
}
