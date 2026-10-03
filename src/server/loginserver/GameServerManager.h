//////////////////////////////////////////////////////////////////////
//
// Filename    : GameServerManager.h
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __GAME_SERVER_MANANGER_H__
#define __GAME_SERVER_MANANGER_H__

// include files
#include "DatagramSocket.h"
#include "Exception.h"
#include "ManagedThread.h"
#include "Types.h"

class Datagram;
class DatagramPacket;

//////////////////////////////////////////////////////////////////////
//
// class GameServerManager;
//
// Worker thread dedicated to communication with the game servers.
//
// It owns a single datagram server socket. The socket is nonblocking, so an
// idle link cannot keep the worker from observing a shutdown request.
//
//////////////////////////////////////////////////////////////////////

class GameServerManager : public ManagedThread {
public:
    // constructor
    GameServerManager();

    // destructor
    ~GameServerManager() noexcept;

    //
    void init() {}

    // stop thread
    void stop() override;

    // main method
    void run() override;

    void sendDatagram(Datagram* pDatagram);
    // Normal return means a complete local UDP send. A reported send failure
    // throws ConnectException so callers cannot advance a failed handshake.
    void sendPacket(const string& host, uint port, const DatagramPacket* pPacket);

private:
    // UDP server socket
    DatagramSocket* m_pDatagramSocket;
};

#endif
