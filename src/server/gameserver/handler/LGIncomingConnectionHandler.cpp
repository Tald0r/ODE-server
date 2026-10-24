//----------------------------------------------------------------------
//
// Filename    : LGIncomingConnectionHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include <random>

#include "LGIncomingConnection.h"
#include "Properties.h"

#ifdef __GAME_SERVER__

#include "ConnectionInfo.h"
#include "ConnectionInfoManager.h"
#include "GLIncomingConnectionError.h"
#include "GLIncomingConnectionOK.h"
#include "GameContext.h"
#include "KernelContext.h"
#include "LogDef.h"
#include "LoginServerManager.h"

#endif

//----------------------------------------------------------------------
//
// LGIncomingConnectionHander::execute()
//
// When the game server gets an LGIncomingConnection packet from the login server,
// it adds a new ConnectionInfo.
//
//----------------------------------------------------------------------
void LGIncomingConnectionHandler::execute(LGIncomingConnection* pPacket)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    //--------------------------------------------------------------------------------
    //
    // Generate the authentication key.
    //
    // *NOTE*
    //
    // The old way had the login server generate the authentication key, send it to the game
    // server and then to the client. That generates the key in CLSelectPCHandler::execute()
    // and sends it to the client in GLIncomingConnectionOKHandler::execute(), and since the
    // handling methods differ the key value has to be kept somewhere. The simplest way is to
    // store it on the login player object.. which is clumsy. Another way is to send the key
    // value back from the game server to the login server, which is unnecessary because the
    // key then crosses the network twice.
    //
    // So generating it on the game server and sending it to the login server is far cleaner.
    //
    // *TODO*
    //
    // In the worst case the local network is sniffed and the key value leaks. (then again, the
    // root password could leak too.. that is what SSL is for..)
    // Against that, the GLIncomingConnectionOK packet ought to be encrypted.
    //
    // The key value should also be unpredictable. (reading the code makes it predictable anyway.)
    //
    //--------------------------------------------------------------------------------

    std::random_device entropy;
    const DWORD authKey = std::uniform_int_distribution<DWORD>{}(entropy);

    // Create the CI object.
    ConnectionInfo* pConnectionInfo = new ConnectionInfo();
    pConnectionInfo->setClientIP(pPacket->getClientIP());
    pConnectionInfo->setKey(authKey);
    pConnectionInfo->setPlayerID(pPacket->getPlayerID());
    pConnectionInfo->setPCName(pPacket->getPCName());

    //--------------------------------------------------------------------------------
    //
    // Set the expire time to the current time + 20 seconds.
    //
    // *TODO*
    //
    // The expire period would be better specified in the Config file too.
    //
    //--------------------------------------------------------------------------------
    Timeval currentTime;
    getCurrentTime(currentTime);
    currentTime.tv_sec += 30;
    pConnectionInfo->setExpireTime(currentTime);

    // debug message

    try {
        // Add it to the CIM.
        de::gameContext().connectionInfos().addConnectionInfo(pConnectionInfo);

        // by sigi. 2002.12.7
        FILELOG_INCOMING_CONNECTION("connectionInfo.log", "Add [%s:%s] %s (%u)", pPacket->getPlayerID().c_str(),
                                    pPacket->getPCName().c_str(), pPacket->getClientIP().c_str(), authKey);


        // Report back to the login server.
        GLIncomingConnectionOK glIncomingConnectionOK;
        glIncomingConnectionOK.setPlayerID(pPacket->getPlayerID());
        glIncomingConnectionOK.setTCPPort(de::kernelContext().config().getPropertyInt("TCPPort"));
        glIncomingConnectionOK.setKey(authKey);

        de::gameContext().loginServer().sendPacket(pPacket->getHost(), pPacket->getPort(), &glIncomingConnectionOK);

        cout << "LGIncomingConnectionHandler Send Packet to ServerIP : " << pPacket->getHost() << endl;
        cout << "LGIncomingConnectionHandler Send Packet to ServerPort : " << pPacket->getPort() << endl;
    } catch (DuplicatedException&) {
        // The connection info is discarded; the loginserver is not told.
        SAFE_DELETE(pConnectionInfo);
    }

#endif

    __END_DEBUG_EX
}
