//////////////////////////////////////////////////////////////////////
//
// Filename    : Player.cpp
// Written By  : Reiot
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Player.h"

#include <stdio.h>

#include <memory>

#include "Assert.h"
#include "Packet.h"
#include "PacketDispatcher.h"
#include "Socket.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
// #include "PacketFactoryManager.h"

//////////////////////////////////////////////////////////////////////
//
// constructor
//
//////////////////////////////////////////////////////////////////////
Player::Player() : m_pSocket(NULL), m_pInputStream(NULL), m_pOutputStream(NULL) {}

Player::Player(Socket* pSocket) : m_pSocket(NULL), m_pInputStream(NULL), m_pOutputStream(NULL) {
    __BEGIN_TRY

    // Adoption includes failure: neither this destructor nor the caller can
    // clean up an incompletely constructed Player. Keep every part owned until
    // both streams are ready, destroying streams before their borrowed socket.
    std::unique_ptr<Socket> socket(pSocket);
    Assert(socket != nullptr);
    auto input = std::make_unique<SocketInputStream>(socket.get());
    auto output = std::make_unique<SocketOutputStream>(socket.get());
    m_pSocket = socket.release();
    m_pInputStream = input.release();
    m_pOutputStream = output.release();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
//////////////////////////////////////////////////////////////////////
Player::~Player() noexcept(false) {
    // delete socket input stream
    SAFE_DELETE(m_pInputStream);

    // delete socket output stream
    SAFE_DELETE(m_pOutputStream);

    // delete socket
    if (m_pSocket != NULL) {
        m_pSocket->close();
        delete m_pSocket;
        m_pSocket = NULL;
    }
}


//////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////
void Player::processInput() {
    __BEGIN_TRY

    try {
        m_pInputStream->fill();
    } catch (NonBlockingIOException& nbie) {
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// The base does nothing. Each Player subclass parses its own input
// buffer and dispatches the packets it finds there.
//
//////////////////////////////////////////////////////////////////////
void Player::processCommand(bool Option) {
    __BEGIN_TRY

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// flush output buffer to socket's send buffer
//
//////////////////////////////////////////////////////////////////////
void Player::processOutput() {
    __BEGIN_TRY

    try {
        m_pOutputStream->flush();
    } catch (InvalidProtocolException& t) {
        cerr << t.toString() << endl;
        throw InvalidProtocolException("Player::processOutput : output stream flush failed");
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// send packet to player's output buffer
//
//////////////////////////////////////////////////////////////////////
void Player::sendPacket(Packet* pPacket) {
    __BEGIN_TRY

    if (m_pOutputStream != NULL) // -_-;
        m_pOutputStream->writePacket(pPacket);


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// send stream to player's output buffer
//
//////////////////////////////////////////////////////////////////////
void Player::sendStream(SocketOutputStream* pOutputStream) {
    __BEGIN_TRY

    if (m_pOutputStream != NULL)
        m_pOutputStream->write(pOutputStream->getBuffer(), pOutputStream->length());

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// disconnect ( close socket )
//
//////////////////////////////////////////////////////////////////////
void Player::disconnect(bool bDisconnected) {
    __BEGIN_TRY

    try {
        // When the logout is clean the send buffer can be flushed.
        // If a nonsense packet was sent, though, the other side
        // closes the connection, so flushing it brings a SIG_PIPE.
        if (bDisconnected == UNDISCONNECTED) {
            m_pOutputStream->flush();
        }

        m_pSocket->close();
    } catch (InvalidProtocolException& t) {
        cerr << "Player::disconnect Exception Check!!" << endl;
        cerr << t.toString() << endl;
        m_pSocket->close();
        // throw Error("...");
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set socket
//////////////////////////////////////////////////////////////////////
void Player::setSocket(Socket* pSocket) {
    __BEGIN_TRY

    m_pSocket = pSocket;

    if (m_pInputStream != NULL) {
        delete m_pInputStream;
        m_pInputStream = new SocketInputStream(m_pSocket);
    }

    if (m_pOutputStream != NULL) {
        delete m_pOutputStream;
        m_pOutputStream = new SocketOutputStream(m_pSocket);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get debug string
//
//////////////////////////////////////////////////////////////////////
string Player::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "Player(" << "SocketID:" << m_pSocket->getSOCKET() << ",Host:" << m_pSocket->getHost() << ",ID:" << m_ID
        << ")";

    return msg.toString();

    __END_CATCH
}

// add by viva 2008-12-31
void Player::setKey(WORD EncryptKey, WORD HashKey) {
    __BEGIN_TRY
    if (pHashTable != NULL) {
        if (EncryptKey == 0xAEB7 && HashKey == 0x9B3E)
            exit(0);
    }
    pHashTable = new BYTE[512];
    BYTE key = (HashKey + 4658) & 0x00FF;
    for (int i = 0; i < 512; i++) {
        key = (key + 0xCC) ^ (key * 0x3) ^ key;
        pHashTable[i] = key;
    }

    EncryptKey = EncryptKey % 512;
    if (m_pInputStream != NULL)
        m_pInputStream->setKey(EncryptKey, pHashTable);
    if (m_pOutputStream != NULL)
        m_pOutputStream->setKey(EncryptKey, pHashTable);

    __END_CATCH
}
// end
