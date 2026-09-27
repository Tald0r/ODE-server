/////////////////////////////////////////////////////////////////////////////
// Filename : MPlayer.h
// Desc		: mofus communication object
/////////////////////////////////////////////////////////////////////////////

#ifndef __MPLAYER_H__
#define __MPLAYER_H__

// include files
#include "Exception.h"
#include "Player.h"
#include "Types.h"

// forward declaration
class MPacket;
class MJob;

// class MPlayer
class MPlayer : public Player {
public:
    MPlayer(MJob* pJob);
    virtual ~MPlayer() noexcept;

public:
    // network read / write
    void processInput();
    void processOutput();

    // parse packet and execute handler for the packet
    void processCommand();
    void processCommand(bool Option) {
        Player::processCommand(Option);
    }

    // send packet to player's output buffer
    using Player::sendPacket;
    void sendPacket(MPacket* pPacket);

    // connect
    void connect();

    // disconnect
    void disconnect(bool bDisconnected = DISCONNECTED);

public:
    // process
    void process();

    ///////////////////////////////////////////////
    // send functions
    ///////////////////////////////////////////////
    void sendConnectAsk();
    void sendLogout();
    void sendUserInfo();
    void sendReceiveOK();
    void sendResult();
    void sendSError(int errorCode);

    // get Job
    MJob* getJob() const {
        return m_pJob;
    }

    // add power point
    void addPowerPoint(int point);

    // set errorcode
    void setErrorCode(int errorCode);

    // work finished
    void setEnd(bool bEnd = true);

private:
    // the work to handle
    MJob* m_pJob;
};

#endif
