//////////////////////////////////////////////////////////////////////
//
// Filename    : LoginPlyaer.cpp
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

#include "LoginPlayer.h"

#include <memory>

#include "Assert.h"
#include "DatabaseError.h"
#include "GameServerManager.h"
#include "KernelContext.h"
#include "LCLoginError.h"
#include "LCLoginOK.h"
#include "LGKickCharacter.h"
#include "LoginContext.h"
#include "LoginKickDispatch.h"
#include "LoginKickPreparation.h"
#include "Packet.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketProfile.h"
#include "PacketValidator.h"
#include "Profile.h"
#include "ServerContext.h"
#include "repository/LoginAccountRepository.h"
#include "repository/LoginCharacterRepository.h"

// by sigi. 2002.11.12
const int defaultLoginPlayerInputStreamSize = 1024;
const int defaultLoginPlayerOutputStreamSize = 4096;

static int maxIdleSec = 60 * 15; // disconnect automatically after 15 idle minutes.

// Time check that works around the 'already connected' problem.
static uint maxWaitForKickCharacter = 3; // seconds to wait for the GameServer's answer.


// Function in CLLoginHandler.cpp.
void addLoginPlayerData(const string& ID, const string& ip, const string& SSN, const string& zipcode);


//////////////////////////////////////////////////////////////////////
//
// constructor
//
//////////////////////////////////////////////////////////////////////
LoginPlayer::LoginPlayer(Socket* pSocket)
    : Player(pSocket, defaultLoginPlayerInputStreamSize, defaultLoginPlayerOutputStreamSize), m_PlayerStatus(LPS_NONE),
      m_FailureCount(0) {
    __BEGIN_TRY

    m_Mutex.setName("LoginPlayer");

    m_ID = "NONE";

    Assert(m_PacketHistory.empty());

    // When a login player is created, the current time counts as its last input time.
    getCurrentTime(m_ExpireTime);
    m_ExpireTime.tv_sec += maxIdleSec;

    m_WorldID = 1;
    setServerGroupID(0);

    m_isAdult = true;

    m_bFreePass = false;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// destructor
//
//////////////////////////////////////////////////////////////////////
LoginPlayer::~LoginPlayer() noexcept {
    __BEGIN_TRY

    // Whenever any player object is deleted, its status must be logout.
    // That is, to disconnect a player its status must be set to logout.
    Assert(m_PlayerStatus == LPS_END_SESSION);

    // delete all previous packets
    while (!m_PacketHistory.empty()) {
        Packet* pPacket = m_PacketHistory.front();
        delete pPacket;
        m_PacketHistory.pop_front();
    }

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
//
// For the 'already connected' case. Sets the time to wait for the
// character to be disconnected by force.
//
//////////////////////////////////////////////////////////////////////
void LoginPlayer::setExpireTimeForKickCharacter() {
    Timeval now;
    getCurrentTime(now);
    setExpireTimeForKickCharacter(now);
}

void LoginPlayer::setExpireTimeForKickCharacter(const Timeval& now) noexcept {
    m_ExpireTimeForKickCharacter = now;
    m_ExpireTimeForKickCharacter.tv_sec += maxWaitForKickCharacter;
}

//////////////////////////////////////////////////////////////////////
//
// parse packet and execute handler for the packet
//
//////////////////////////////////////////////////////////////////////
void LoginPlayer::processCommand(bool Option) {
    __BEGIN_TRY


    // For the 'already connected' case, when forcing a disconnect.
    if (m_PlayerStatus == LPS_WAITING_FOR_GL_KICK_VERIFY) {
        Timeval currentTime;
        getCurrentTime(currentTime);

        de::retryLoginKick(
            *this, currentTime, [](LoginPlayer& player) { return player.resendLGKickCharacter(); },
            [](LoginPlayer& player) { player.sendLCLoginOK(); });

        return;
    }

    try {
        // Create a buffer to hold the header temporarily
        char header[szPacketHeader];
        PacketID_t packetID;
        PacketSize_t packetSize;

        PacketFactoryManager& packetFactories = de::kernelContext().packetFactories();

        // Process every complete packet sitting in the input buffer.
        while (true) {
            // Read as many bytes as the packet header from the input stream.
            // If the requested number of bytes cannot be read from the stream,
            // an Insufficient exception is thrown and the loop is left.
            if (!m_pInputStream->peek(header, szPacketHeader)) {
                // If there was no input at all, check whether the input timeout expired.
                Timeval currentTime;
                getCurrentTime(currentTime);
                if (currentTime >= m_ExpireTime)
                    throw DisconnectException("Connection closed after a period with no input.");
                break;
            }

            // Work out the packet id and the packet size.
            // The packet size includes the header here.
            memcpy(&packetID, &header[0], szPacketID);
            memcpy(&packetSize, &header[szPacketID], szPacketSize);

            // DEBUG by tiancaiamao
            StringStream msg;
            msg << "RECV PACKET from " << m_ID << ", " << packetFactories.getPacketName(packetID) << "(" << packetID
                << ") " << szPacketHeader + packetSize << "/" << m_pInputStream->length() << eos;
            cout << msg.toString() << endl;

            // A strange packet id counts as a protocol error.
            if (packetID >= Packet::PACKET_MAX)
                // Spell the error out for debugging.
                throw InvalidProtocolException("too large packet id");

            try {
                // Check that the packet order is valid.
                if (!de::kernelContext().packetValidator().isValidPacketID(getPlayerStatus(), packetID)) {
                    // DEBUG by tiancaiamao
                    cout << "player status: " << getPlayerStatus() << " receive packet: " << packetID << endl;
                    throw InvalidProtocolException("invalid packet order");
                }

                // A packet size that is too large counts as a protocol error.
                if (packetSize > packetFactories.getPacketMaxSize(packetID))
                    throw InvalidProtocolException("too large packet size");

                // Check that the input buffer holds as many bytes as the packet size.
                // break could be used when optimizing. (an exception is used here for now.)
                if (m_pInputStream->length() < szPacketHeader + packetSize)
                    //	throw InsufficientDataException();
                    break;

                // Update the last input time.
                // The last input time is when one complete packet arrived.
                getCurrentTime(m_ExpireTime);
                m_ExpireTime.tv_sec += maxIdleSec;

                // Getting here means the input buffer holds at least one complete packet.
                // The packet structure can be created from the packet factory manager with the packet id.
                // A wrong packet id is handled by the packet factory manager.
                // The packet is owned here until the history takes it, so a
                // read() or a handler that throws does not leak it.
                std::unique_ptr<Packet> pPacket(packetFactories.createPacket(packetID));

                // Now initialize this packet structure.
                // The read() defined in the packet subclass is called through the virtual
                // mechanism, so it is initialized automatically.
                m_pInputStream->readPacket(pPacket.get());

                Timeval start, end;
                getCurrentTime(start);

                // Now the packet handler can be run with this packet structure.
                // A wrong packet id is handled by the packet handler manager.
                PacketDispatcher::dispatch(pPacket.get(), this);

                getCurrentTime(end);
                g_PacketProfileManager.addAccuTime(pPacket->getPacketName(), start, end);

                // Put the current packet at the end of the packet history,
                // which owns it from here on.
                m_PacketHistory.push_back(pPacket.get());
                (void)pPacket.release();

                // Keep only nPacketHistory packets.
                while (m_PacketHistory.size() > nPacketHistory) {
                    Packet* oldPacket = m_PacketHistory.front();
                    delete oldPacket;
                    m_PacketHistory.pop_front();
                }
            } catch (IgnorePacketException&) {
                // The PacketValidator said to ignore the packet, so
                // drop it from the input stream and do not execute it.

                // A packet size that is too large counts as a protocol error.
                if (packetSize > packetFactories.getPacketMaxSize(packetID))
                    throw InvalidProtocolException("too large packet size");

                // Check that the input buffer holds as many bytes as the packet size.
                // break could be used when optimizing. (an exception is used here for now.)
                if (m_pInputStream->length() < szPacketHeader + packetSize)
                    throw InsufficientDataException();

                // Once all the data has arrived, skip that many bytes and
                // move on to the next packet....
                m_pInputStream->skip(szPacketHeader + packetSize);

                // An ignored packet does not affect expiry.
                // That is, only a valid packet is kept from being cut off.
                // It does not go into the history either.
            }
        }
    } catch (InsufficientDataException& ide) {
        // If there was no input at all, check whether the input timeout expired.
        Timeval currentTime;
        getCurrentTime(currentTime);
        if (currentTime >= m_ExpireTime)
            throw DisconnectException("Connection closed after a period with no input.");
    } catch (InvalidProtocolException& ipe) {
        // The connection has to be closed by force. By what means??
        throw;
    } catch (DisconnectException& de) {
        // Some problem in packet processing means the connection must be closed.
        throw;
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// disconnect player
//--------------------------------------------------------------------------------
void LoginPlayer::disconnect(bool bDisconnected) {
    __BEGIN_TRY

    if (bDisconnected == UNDISCONNECTED) {
        // Send whatever data is left in the output buffer.
        m_pOutputStream->flush();
    }

    // Close the socket connection.
    m_pSocket->close();

    // The 'already connected' case, waiting for the character to be kicked.
    if (m_PlayerStatus == LPS_WAITING_FOR_GL_KICK_VERIFY) {
        m_ID = "NONE";
    }

    // Set the player's status to logout.
    Assert(m_PlayerStatus != LPS_END_SESSION);
    m_PlayerStatus = LPS_END_SESSION;

    // Having an id set means the login went through.
    // In the 'already connected' case..
    // Not while waiting for the character to be kicked, since the ID may be set then
    if (m_ID != "NONE") {
        try {
            defaultLoginAccountRepository().markLoggedOff(m_ID);
        } catch (const DatabaseError& error) {
            // A SQL failure arrives as END_DB's DatabaseError carrying the
            // line it wrote to DBError.log; rethrown as the Error the callers
            // expect, with that line in it.
            throw Error("LoginPlayer::disconnect : " + error.message());
        }
    }

    __END_CATCH
}
//--------------------------------------------------------------------------------
// disconnect player no log
// Keep the log out of the DB.
//--------------------------------------------------------------------------------
void LoginPlayer::disconnect_nolog(bool bDisconnected) {
    __BEGIN_TRY

    if (bDisconnected == UNDISCONNECTED) {
        // Send whatever data is left in the output buffer.
        m_pOutputStream->flush();
    }

    // Close the socket connection.
    m_pSocket->close();

    // The 'already connected' case, waiting for the character to be kicked.
    if (m_PlayerStatus == LPS_WAITING_FOR_GL_KICK_VERIFY) {
        m_ID = "NONE";
    }

    // Set the player's status to logout.
    Assert(m_PlayerStatus != LPS_END_SESSION);
    m_PlayerStatus = LPS_END_SESSION;

    // Having an id set means the login went through.
    // In the 'already connected' case..
    // Not while waiting for the character to be kicked, since the ID may be set then
    if (m_ID != "NONE") {
        try {
            defaultLoginAccountRepository().markLoggedOff(m_ID);
        } catch (const DatabaseError& error) {
            // A SQL failure arrives as END_DB's DatabaseError carrying the
            // line it wrote to DBError.log; rethrown as the Error the callers
            // expect, with that line in it.
            throw Error("LoginPlayer::disconnect : " + error.message());
        }
    }


    __END_CATCH
}

//--------------------------------------------------------------------------------
//
// Originally no thread other than the login player manager was meant to touch
// a login player at the same time, but the game server manager runs as its own
// thread and can now reach the login player as well. So
// the mutex-protected version below was put together.
//
//--------------------------------------------------------------------------------
void LoginPlayer::sendPacket(Packet* pPacket) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Player::sendPacket(pPacket);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the N-th most recent packet.
//
// N == 0 returns the most recent packet.
//
// Up to nPacketHistory - 1 can be given.
//
//////////////////////////////////////////////////////////////////////
Packet* LoginPlayer::getOldPacket(uint prev) {
    __BEGIN_TRY

    if (prev >= nPacketHistory)
        throw OutOfBoundException();
    if (prev >= m_PacketHistory.size())
        throw NoSuchElementException();
    return m_PacketHistory[m_PacketHistory.size() - prev - 1];

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the most recent packet with a given packet id.
//
//////////////////////////////////////////////////////////////////////
Packet* LoginPlayer::getOldPacket(PacketID_t packetID) {
    __BEGIN_TRY

    Packet* pPacket = NULL;

    deque<Packet*>::reverse_iterator ritr = m_PacketHistory.rbegin();
    for (; ritr != m_PacketHistory.rend(); ritr++) {
        if ((*ritr)->getPacketID() == packetID) {
            pPacket = (*ritr);
            break;
        }
    }

    if (pPacket == NULL)
        throw NoSuchElementException();

    return pPacket;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//
// send LGKickCharacter
//
// Ask the GameServer to remove the character that is 'already connected'.
//
//////////////////////////////////////////////////////////////////////////////
void LoginPlayer::sendLGKickCharacter() {
    de::beginLoginKick(*this, [](LoginPlayer& player) { return player.resendLGKickCharacter(); });
}

bool LoginPlayer::resendLGKickCharacter() {
    cout << "send LGKickCharacter" << endl;

    const auto target = de::prepareLoginKick(*this, defaultLoginAccountRepository(), defaultLoginCharacterRepository());
    if (!target)
        return false;

    try {
        const bool sent = de::dispatchLoginKick(*this, *target, de::serverContext().serverInfos(),
                                                [](const std::string& host, uint port, const LGKickCharacter& packet) {
                                                    de::loginContext().gameServers().sendPacket(host, port, &packet);
                                                });
        // Destination refusal already suppressed the identity. End this wait so
        // the client can retry login and ordinary idle expiry resumes.
        if (!sent)
            setPlayerStatus(LPS_BEGIN_SESSION);
        return sent;
    } catch (const ConnectException&) {
        // The manager disconnects this failed attempt. Its identity must not
        // mark the account's existing game session LOGOFF, even on the first send.
        setID("NONE");
        throw;
    }
}


//////////////////////////////////////////////////////////////////////
//
// send LCLoginOK
//
// Set Player table LogOn to 'LOGON' and
// send LCLoginOK to the client.
// PlayerStatus is set to LPS_WAITING_FOR_CL_GET_PC_LIST.
//
//////////////////////////////////////////////////////////////////////
void LoginPlayer::sendLCLoginOK() {
    try {
        string connectIP = getSocket()->getHost();

        // LogOn flips to LOGON; a row that did not change belongs to a
        // session already logged on.
        if (!defaultLoginAccountRepository().setLoggedOn(getID())) {
            filelog("MultiLogin.log", "Multiple login attempt suspected : [%s:%s]", getID().c_str(), connectIP.c_str());

            LCLoginError lcLoginError;
            lcLoginError.setErrorID(ALREADY_CONNECTED);
            sendPacket(&lcLoginError);

            setPlayerStatus(LPS_BEGIN_SESSION);
            return;
        }

        defaultLoginAccountRepository().setLoginIP(connectIP, getID());

        LCLoginOK lcLoginOK;
        lcLoginOK.setAdult(isAdult());
        lcLoginOK.setLastDays(0xffff);

        sendPacket(&lcLoginOK);

        setPlayerStatus(LPS_WAITING_FOR_CL_GET_PC_LIST);

        addLoginPlayerData(m_ID, connectIP, m_SSN, m_Zipcode);
    } catch (Throwable& t) {
        filelog("loginOKError.txt", "%s", t.toString().c_str());
        throw;
    }
}


//////////////////////////////////////////////////////////////////////
//
// get debug string
//
//////////////////////////////////////////////////////////////////////
string LoginPlayer::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "LoginPlayer(" << "ID:" << m_ID << ",SocketID:" << m_pSocket->getSOCKET() << ",Host:" << m_pSocket->getHost()
        << ")";
    return msg.toString();

    __END_CATCH
}
