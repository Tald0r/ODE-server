//////////////////////////////////////////////////////////////////////////////
// Filename    : GamePlyaer.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GamePlayer.h"

#include <stdio.h>

#include <fstream>
#include <memory>

#include "Assert.h"
#include "CGConnect.h"
#include "Creature.h"
#include "Deployment.h"
#include "EventKick.h"
#include "GCKickMessage.h"
#include "GCSystemMessage.h"
#include "GSGuildMemberLogOn.h"
#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "IncomingPlayerManager.h"
#include "KernelContext.h"
#include "Ousters.h"
#include "PCFinder.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerMailbox.h"
#include "Properties.h"
#include "RelicUtil.h"
#include "SharedServerManager.h"
#include "Slayer.h"
#include "StringPool.h"
#include "TelephoneCenter.h"
#include "Thread.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "repository/SessionRepository.h"


//////////////////////////////////////////////////////////////////////////////
// Profile every packet. by sigi. 2002.5.6
//
// Using it requires
// MAX_PROFILE_SAMPLES += 300 in Profile.h.
//////////////////////////////////////////////////////////////////////////////
// #define __PROFILE_PACKETS__

#ifdef __PROFILE_PACKETS__
#include "Profile.h"
#endif

// by sigi. 2002.11.12
const int defaultGamePlayerInputStreamSize = 1024;
const int defaultGamePlayerOutputStreamSize = 20480;

static int maxIdleSec = 60 * 5;  // Idle connections are kept this long: 60 * 5 = 300 seconds (5 minutes)
static int maxVerifyCount = 3;   // Early heartbeats carried before the next one is refused: 3
static int maxTimeGap = 5;       // How far ahead of the interval a heartbeat may arrive, in seconds
static int SpeedCheckDelay = 60; // The interval the client sends its heartbeat on, in seconds

const int PCRoomLottoSec = 3600;    // 3600 seconds. 1 hour
const int PCRoomLottoMaxAmount = 3; // Maximum number of lottery tickets that can accumulate at once


//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

GamePlayer::GamePlayer(Socket* pSocket)
    : // Player(pSocket), 	// by sigi. 2002.11.12
      m_pCreature(NULL), m_PlayerStatus(GPS_NONE), m_pReconnectPacket(NULL) {
    __BEGIN_TRY

    Assert(pSocket != NULL);
    m_pSocket = pSocket;

#ifdef __USE_ENCRYPTER__
    // create socket input stream
    m_pInputStream = new SocketEncryptInputStream(m_pSocket, defaultGamePlayerInputStreamSize);
    Assert(m_pInputStream != NULL);

    // create socket output stream
    m_pOutputStream = new SocketEncryptOutputStream(m_pSocket, defaultGamePlayerOutputStreamSize);
    Assert(m_pOutputStream != NULL);
#else
    // create socket input stream
    m_pInputStream = new SockettInputStream(m_pSocket, defaultGamePlayerInputStreamSize);
    Assert(m_pInputStream != NULL);

    // create socket output stream
    m_pOutputStream = new SockettOutputStream(m_pSocket, defaultGamePlayerOutputStreamSize);
    Assert(m_pOutputStream != NULL);
#endif

    m_Mutex.setName("GamePlayer");

    getCurrentTime(m_ExpireTime);
    m_ExpireTime.tv_sec += maxIdleSec;

    m_SpecialEventCount = 0;

    m_bKickForLogin = false;

    m_bFreePass = false;

    m_ItemRatioBonusPoint = 0;

    m_PCRoomLottoStartTime.tv_sec = 0;
    m_PCRoomLottoStartTime.tv_usec = 0;
    m_PCRoomLottoSumTime = 0;

    m_bPacketLog = false;

    // The construction time counts as the login time.
    m_LoginDateTime = VSDateTime::currentDateTime();

    //	m_NProtectCSAuth.Init();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////

GamePlayer::~GamePlayer() noexcept {
    __BEGIN_TRY

    GuildManager& guilds = de::gameContext().guilds();
    SharedServerManager& sharedServer = de::gameContext().sharedServer();

    //__ENTER_CRITICAL_SECTION(m_Mutex)

    // Whatever deletes a player object, its status has to be logged out.
    // So ending a player session means putting it in the logged-out state.
    Assert(m_PlayerStatus == GPS_END_SESSION);

    try {
        // Delete creature
        if (m_pCreature != NULL) {
            // Drop the relic. A drop that fails (no free tile) is logged and
            // the relic goes with the creature: the session still ends, the
            // finder removal below included, so no name outlives its player.
            if (m_pCreature->hasRelicItem()) {
                try {
                    dropRelicToZone(m_pCreature, false);
                } catch (Throwable& t) {
                    filelog("WarError.log", "%s logged out holding a relic that could not be dropped: %s",
                            m_pCreature->getName().c_str(), t.toString().c_str());
                }
            }

            dropFlagToZone(m_pCreature, false);
            dropSweeperToZone(m_pCreature);

            de::gameContext().playerCreatures().deleteCreature(m_pCreature->getName());

            // From here on postToPlayer() cannot find this player, so what
            // is still queued for it runs its ifGone handlers now. Each
            // handler's own failure is logged inside; the try covers the
            // drain itself (an allocation), since this destructor is noexcept.
            try {
                de::abandonPlayerMailbox(*this);
            } catch (...) {
            }


            // Remove from the guild list of currently connected members.
            if (m_pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(m_pCreature);
                if (pSlayer->getGuildID() != 99) {
                    Guild* pGuild = guilds.getGuild(pSlayer->getGuildID());
                    if (pGuild != NULL) {
                        pGuild->deleteCurrentMember(pSlayer->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pSlayer->getName());
                        gsGuildMemberLogOn.setLogOn(false);

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // Update the database.
                        defaultSessionRepository().markGuildMemberLoggedOff(pSlayer->getName());
                    } else
                        filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n",
                                (int)pSlayer->getGuildID(), pSlayer->getName().c_str());
                }
            } else if (m_pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(m_pCreature);
                if (pVampire->getGuildID() != 0) {
                    Guild* pGuild = guilds.getGuild(pVampire->getGuildID());
                    if (pGuild != NULL) {
                        pGuild->deleteCurrentMember(pVampire->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pVampire->getName());
                        gsGuildMemberLogOn.setLogOn(false);

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // Update the database.
                        defaultSessionRepository().markGuildMemberLoggedOff(pVampire->getName());
                    } else
                        filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n",
                                (int)pVampire->getGuildID(), pVampire->getName().c_str());
                }
            } else if (m_pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(m_pCreature);
                if (pOusters->getGuildID() != 66) {
                    Guild* pGuild = guilds.getGuild(pOusters->getGuildID());
                    if (pGuild != NULL) {
                        pGuild->deleteCurrentMember(pOusters->getName());

                        GSGuildMemberLogOn gsGuildMemberLogOn;
                        gsGuildMemberLogOn.setGuildID(pGuild->getID());
                        gsGuildMemberLogOn.setName(pOusters->getName());
                        gsGuildMemberLogOn.setLogOn(false);

                        sharedServer.sendPacket(&gsGuildMemberLogOn);

                        // Update the database.
                        defaultSessionRepository().markGuildMemberLoggedOff(pOusters->getName());
                    } else
                        filelog("GuildMissing.log", "[NoSuchGuild] GuildID : %d, Name : %s\n",
                                (int)pOusters->getGuildID(), pOusters->getName().c_str());
                }
            }

            int PartyID = m_pCreature->getPartyID();

            if (PartyID != 0) {
                Zone* pZone = m_pCreature->getZone();
                if (pZone != NULL) {
                    LocalPartyManager* pLocalPartyManager = pZone->getLocalPartyManager();
                    pLocalPartyManager->deletePartyMember(PartyID, m_pCreature);
                }
            }

            SAFE_DELETE(m_pCreature);
            // m_pCreature->setPlayer( NULL );
        }
    } catch (NoSuchElementException& nsee) {
        cerr << "GamePlayer::~GamePlayer() : " << nsee.toString() << endl;
        throw Error("GamePlayer::~GamePlayer() : NoSuchElementException");
    } catch (Throwable& t) {
        t.addStack();
        throw;
    }

    // Delete the packets.
    while (!m_PacketHistory.empty()) {
        Packet* pPacket = m_PacketHistory.front();
        SAFE_DELETE(pPacket);
        m_PacketHistory.pop_front();
    }

    SAFE_DELETE(m_pReconnectPacket);

    //__LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH_NO_RETHROW
}
//////////////////////////////////////////////////////////////////////
//
// parse packet and execute handler for the packet
//
//////////////////////////////////////////////////////////////////////
void GamePlayer::processCommand(bool Option) {
    __BEGIN_TRY

    Packet* pPacket = NULL;

    PacketFactoryManager& packetFactories = de::kernelContext().packetFactories();
    PacketValidator& packetValidator = de::kernelContext().packetValidator();

    try {
        // Handle a user under penalty here.
        // A penalised user can hit any exception,
        // so decide whether the current ID connection has to be dropped.
        if (isPenaltyFlag(PENALTY_TYPE_KICKED)) {
            filelog("GamePlayer.txt", "Penalty Kicked. Name[%s],Host[%s],Type[%d]",
                    ((getCreature() == NULL) ? "NULL" : getCreature()->getName().c_str()),
                    ((getSocket() == NULL) ? "NULL" : getSocket()->getHost().c_str()), m_ItemRatioBonusPoint);

            throw DisconnectException("He is had penalty");
        }

        if (Option) {
            m_EventManager.heartbeat();
        }

        // Process every complete packet in the input buffer.
        while (true) {
            // The gate decides the frame at the front of the input stream
            // and, once it is whole and admitted, consumes it: an admitted
            // packet is read, an ignored one skipped.
            de::GameFrame frame =
                m_FrameGate.next(*m_pInputStream, getPlayerStatus(), packetFactories, packetValidator);

            // Fewer bytes than a header: wait for more, unless the
            // connection has been idle too long.
            if (frame.step == de::GameFrameStep::AwaitHeader) {
                Timeval currentTime;
                getCurrentTime(currentTime);
                if (currentTime >= m_ExpireTime) {
                    filelog("GamePlayer.txt", "Timeout Disconnect1. Name[%s],Host[%s]",
                            ((getCreature() == NULL) ? "NULL" : getCreature()->getName().c_str()),
                            ((getSocket() == NULL) ? "NULL" : getSocket()->getHost().c_str()));

                    throw DisconnectException("Connection closed after a period with no input.");
                }

                break;
            }

            // The whole packet body has not arrived yet.
            if (frame.step == de::GameFrameStep::AwaitBody)
                break;

            // Nor has an ignored packet's; the idle check below applies.
            if (frame.step == de::GameFrameStep::AwaitIgnoredBody)
                throw InsufficientDataException();

            if (frame.step == de::GameFrameStep::Refused)
                refuseFrame(frame);

            // PacketValidator asked for the packet to be ignored, and the
            // gate dropped it from the input stream unread. An ignored
            // packet does not push the expire time out, so only valid
            // packets keep the connection alive. It does not enter the
            // history either.
            if (frame.step == de::GameFrameStep::Skipped)
                continue;

            // Current time
            getCurrentTime(m_ExpireTime);
            m_ExpireTime.tv_sec += maxIdleSec;

            // Append the packet to the end of the packet history, which
            // owns it from here on.
            m_PacketHistory.push_back(frame.packet.get());
            pPacket = frame.packet.release();

            // Write the packet file log.
            if (m_bPacketLog) {
                Timeval currentTime;
                getCurrentTime(currentTime);

                if (currentTime >= m_PacketLogEndTime) {
                    m_bPacketLog = false;
                } else {
                    filelog(m_PacketLogFileName.c_str(), "%s", pPacket->toString().c_str());
                }
            }

            // cout << "[" << (int)Thread::self() << "] execute before : " << pPacket->getPacketName().c_str() <<
            // endl;

            // Run the packet handler for this packet structure.
            // A bad packet id is handled inside the packet handler manager.
            try {
#ifdef __PROFILE_PACKETS__

                beginProfileEx(pPacket->getPacketName().c_str());
                PacketDispatcher::dispatch(pPacket, this);
                endProfileEx(pPacket->getPacketName().c_str());

#else
                PacketDispatcher::dispatch(pPacket, this);
#endif
            } catch (...) {
                filelog("GamePlayerError.txt", "Player:[%s], IP:[%s],MAC:[%02x%02x%02x%02x%02x%02x],Packet is:%s",
                        m_ID.c_str(), getSocket()->getHost().c_str(), m_MacAddress[0], m_MacAddress[1], m_MacAddress[2],
                        m_MacAddress[3], m_MacAddress[4], m_MacAddress[5], pPacket->toString().c_str());
                throw DisconnectException("GamePlayer Error 2!");
            }
            // cout << "[" << (int)Thread::self() << "] execute after : " << pPacket->getPacketName().c_str() <<
            // endl;

            // Keep only nPacketHistorySize packets.
            while (m_PacketHistory.size() > nPacketHistorySize) {
                Packet* oldPacket = m_PacketHistory.front();
                SAFE_DELETE(oldPacket);
                m_PacketHistory.pop_front();
            }

            // CGReady's handler runs on the MAIN thread (this loop, called
            // from IncomingPlayerManager with Option == false), hands the
            // player to the zone pipeline and flips the status to
            // GPS_NORMAL -- which opens PacketValidator's in-game gate.
            // Packets a client pipelined behind CGReady must not keep
            // draining here: they would dispatch on the main thread and
            // reach the Zone mutation gateways with no group mutex held
            // (a client-triggerable race). Stop;
            // the zone thread's ZonePlayerManager drains the remainder on
            // its next tick.
            if (!Option && getPlayerStatus() == GPS_NORMAL)
                break;
        }
    } catch (InsufficientDataException& ide) {
        // Close the connection once the expire time has passed.
        Timeval currentTime;
        getCurrentTime(currentTime);
        if (currentTime >= m_ExpireTime) {
            filelog("GamePlayer.txt", "Timeout Diconnect. Name[%s],Host[%s]",
                    ((getCreature() == NULL) ? "NULL" : getCreature()->getName().c_str()),
                    ((getSocket() == NULL) ? "NULL" : getSocket()->getHost().c_str()));

            throw DisconnectException("Connection closed after a period with no input.");
        }
    }
    // Commented out. by sigi. 2002.5.14

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// log a refused frame and drop the connection
//
//////////////////////////////////////////////////////////////////////
void GamePlayer::refuseFrame(const de::GameFrame& frame) {
    const char* logFile = de::refusalLogFile(frame.refusal);
    switch (frame.refusal) {
    case de::GameFrameRefusal::OutOfSequence:
        filelog(logFile, "Timeout Disconnect1. Name[%s],Host[%s]",
                ((getCreature() == NULL) ? "NULL" : getCreature()->getName().c_str()),
                ((getSocket() == NULL) ? "NULL" : getSocket()->getHost().c_str()));
        break;

    case de::GameFrameRefusal::IdOutOfRange:
        filelog(logFile, "Packet ID exceed MAX, RECV [%d/%d],ID[%s],Host[%s]", frame.id, Packet::PACKET_MAX,
                m_ID.c_str(), getSocket()->getHost().c_str());
        break;

    case de::GameFrameRefusal::InvalidOrder:
    case de::GameFrameRefusal::StoreInfo:
        filelog(logFile, "Not Valid Packet, RECV [%d],ID[%s],Host[%s]", frame.id, m_ID.c_str(),
                getSocket()->getHost().c_str());
        break;

    case de::GameFrameRefusal::TooLarge: {
        PacketFactoryManager& packetFactories = de::kernelContext().packetFactories();
        filelog(logFile, "Too Larget Packet Size, RECV [%d],PacketSize[%d/%d],ID[%s],Host[%s]", frame.id, frame.size,
                packetFactories.getPacketMaxSize(frame.id), m_ID.c_str(), getSocket()->getHost().c_str());
        break;
    }

    case de::GameFrameRefusal::IgnoredTooLarge:
        filelog(logFile, "Too Larget Packet Size[Ignore], RECV [%d],PacketSize[%d],Name[%s],Host[%s]", frame.id,
                frame.size, ((getCreature() == NULL) ? "NULL" : getCreature()->getName().c_str()),
                ((getSocket() == NULL) ? "NULL" : getSocket()->getHost().c_str()));
        break;

    case de::GameFrameRefusal::None:
        break;
    }
    de::throwRefusal(frame.refusal);
}


//////////////////////////////////////////////////////////////////////
//
// flush output buffer to socket's send buffer
//
// No other thread may call sendPacket on the output buffer while it flushes.
// (The only such case is a say arriving over inter-server communication.)
//
//////////////////////////////////////////////////////////////////////
void GamePlayer::processOutput() {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        Player::processOutput();
    } catch (InvalidProtocolException& It) {
        throw DisconnectException("Pipe broken; closing the connection");
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// send packet to player's output buffer
//
//////////////////////////////////////////////////////////////////////
void GamePlayer::sendPacket(Packet* pPacket) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        // Write the packet file log.
        if (m_bPacketLog) {
            Timeval currentTime;
            getCurrentTime(currentTime);

            if (currentTime >= m_PacketLogEndTime) {
                m_bPacketLog = false;
            } else {
                filelog(m_PacketLogFileName.c_str(), "%s", pPacket->toString().c_str());
            }
        }

        Player::sendPacket(pPacket);

        // cout << "GamePlayer::sendPacket() : " << pPacket->toString() << endl;
        // cout << "GamePlayer::sendPacket() PACKET SIZE : " << pPacket->getPacketSize() << endl;

    } catch (InvalidProtocolException& It) {
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//--------------------------------------------------------------------------------
//
// disconnect player
//
// (1) If the creature exists, remove it from the zone and broadcast.
// (2) Save the creature.
//
//--------------------------------------------------------------------------------
void GamePlayer::disconnect(bool bDisconnected) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    //--------------------------------------------------------------------------------
    // Remove the creature from the zone, then broadcast to the nearby PCs.
    // Then save the creature to the database.
    //--------------------------------------------------------------------------------
    string CreatureName = "";
    if (m_pCreature != NULL) {
        CreatureName = m_pCreature->getName();

        try {
            // The creature is inside a zone only in GPS_NORMAL.
            // *CAUTION*
            // One could worry about the connection ending after pushPC() is called.
            // The order of the processing routines makes that worry unnecessary:
            // even when the connection drops, the zone runs first, the creature reaches
            // its real tile, and only then does the player processing loop come round.
            if (getPlayerStatus() == GPS_NORMAL) {
                //----------------------------------
                // Remove from the zone and broadcast.
                //----------------------------------
                Zone* pZone = m_pCreature->getZone();
                Assert(pZone != NULL);
                pZone->deleteQueuePC(m_pCreature);
                pZone->deleteCreature(m_pCreature, m_pCreature->getX(), m_pCreature->getY());
                //--------------------------------------------------------------------------------
                // Save the creature.
                //--------------------------------------------------------------------------------
                m_pCreature->save();
            }
        } catch (Throwable& t) {
        }
    }

    setPlayerStatus(GPS_END_SESSION);

    //--------------------------------------------------------------------------------
    // A set id means the login happened.
    //--------------------------------------------------------------------------------
    if (m_ID != "") {
        // Only a session still in GAME flips to LOGOFF. by sigi. 2002.5.15
        defaultSessionRepository().markPlayerLoggedOff(m_ID);

        // Delete the IP record.
        defaultSessionRepository().deleteUserIP(CreatureName);
    }

    // Tell the client to go to the login server.
    // Originally handled in LGIncomingConnectionOKHandler. by sigi. 2002.6.19
    if (m_pReconnectPacket != NULL) {
        // cout << "[SendReconnect] " << m_pReconnectPacket->toString().c_str() << endl;

        try {
            // sendPacket( m_pReconnectPacket );
            Player::sendPacket(m_pReconnectPacket);
            // Send whatever is left in the output buffer.
            m_pOutputStream->flush();
        } catch (Throwable& t) {
            // Ignore
        }

        SAFE_DELETE(m_pReconnectPacket);
    }


    // Handled higher up, but timing (LogOn, UserIPInfo and the like) means
    // the connection is cut here and the reconnect packet sent.
    if (bDisconnected == UNDISCONNECTED) {
        try {
            // Send a GCDisconnect packet to the client.
            // GCDisconnect gcDisconnect;
            // sendPacket(gcDisconnect);

            // Send whatever is left in the output buffer.
            m_pOutputStream->flush();
        } catch (Throwable& t) {
            // cerr << "GamePlayer::disconnect() : GamePlayer::disconnect Exception Check!!" << endl;
            // cerr << t.toString() << endl;
        }
    }

    // Close the socket connection.
    m_pSocket->close();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the Nth most recent packet.
//
// N == 0 returns the most recent packet.
//
// N may be at most nPacketHistorySize - 1.
//
//////////////////////////////////////////////////////////////////////
Packet* GamePlayer::getOldPacket(uint prev) {
    __BEGIN_TRY

    if (prev >= nPacketHistorySize)
        throw OutOfBoundException();

    // if prev == 0 , return m_PacketHistory[9]
    // if prev == 9 , return m_PacketHistory[0]
    Packet* pPacket = m_PacketHistory[nPacketHistorySize - prev - 1];

    if (pPacket == NULL)
        throw NoSuchElementException("packet history is empty");

    return pPacket;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Return the most recent packet with the given packet id.
//
//////////////////////////////////////////////////////////////////////
Packet* GamePlayer::getOldPacket(PacketID_t packetID) {
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
        throw NoSuchElementException("packet history is empty");

    return pPacket;

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void GamePlayer::addEvent(Event* pEvent) {
    __BEGIN_TRY

    m_EventManager.addEvent(pEvent);

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void GamePlayer::deleteEvent(Event::EventClass EClass) {
    __BEGIN_TRY

    m_EventManager.deleteEvent(EClass);

    __END_CATCH
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
Event* GamePlayer::getEvent(Event::EventClass EClass) {
    __BEGIN_TRY

    return m_EventManager.getEvent(EClass);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get debug string
//
//////////////////////////////////////////////////////////////////////
string GamePlayer::toString() const {
    __BEGIN_TRY

    StringStream msg;

    //////////////////////////////////////////////////
    // enter critical section
    //////////////////////////////////////////////////
    __ENTER_CRITICAL_SECTION(m_Mutex)

    msg << "GamePlayer(SocketID:" << m_pSocket->getSOCKET() << ",Host:" << m_pSocket->getHost() << ")";

    //////////////////////////////////////////////////
    // leave critical section
    //////////////////////////////////////////////////
    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return msg.toString();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// verifySpeed
//
//////////////////////////////////////////////////////////////////////
// Answers the client heartbeat this player just sent: the clock the packet
// arrived on is the whole input, so the rule itself is decided by
// verifyHeartbeat and only the session's state lives here. The one caller is
// the CGVerifyTime handler, which disconnects the player on a false.
bool GamePlayer::verifySpeed() {
    __BEGIN_TRY

    Timeval CurrentTime;
    getCurrentTime(CurrentTime);

    de::HeartbeatVerifyParams params;
    params.checkDelaySec = SpeedCheckDelay;
    params.maxTimeGapSec = maxTimeGap;
    params.maxEarlyCount = maxVerifyCount;

    return de::verifyHeartbeat(params, CurrentTime.tv_sec, m_SpeedVerify);

    __END_CATCH
}

void GamePlayer::loadSpecialEventCount(void) {
    __BEGIN_TRY

    DWORD count = 0;
    if (!defaultSessionRepository().loadSpecialEventCount(m_ID, count)) {
        throw Error("GamePlayer::loadSpecialEventCount() : unable to dispatch data");
    }
    m_SpecialEventCount = count;

    __END_CATCH
}

void GamePlayer::saveSpecialEventCount(void) {
    __BEGIN_TRY

    defaultSessionRepository().saveSpecialEventCount(m_SpecialEventCount, m_ID);

    __END_CATCH
}

// Set the encryption code.
void GamePlayer::setEncryptCode() {
    __BEGIN_TRY

#ifdef __USE_ENCRYPTER__
    Assert(m_pCreature != NULL);

    uchar code = m_pCreature->getZone()->getEncryptCode();

    SocketEncryptOutputStream* pEOS = dynamic_cast<SocketEncryptOutputStream*>(m_pOutputStream);
    Assert(pEOS != NULL);

    SocketEncryptInputStream* pEIS = dynamic_cast<SocketEncryptInputStream*>(m_pInputStream);
    Assert(pEIS != NULL);

    pEOS->setEncryptCode(code);
    pEIS->setEncryptCode(code);
//	}
#endif

    __END_CATCH
}

void GamePlayer::kickPlayer(uint nSeconds, uint KickMessageType) {
    __BEGIN_TRY

    // Ignore it when an EventKick already exists.
    if (m_EventManager.getEvent(Event::EVENT_CLASS_KICK) != NULL)
        return;

    EventKick* pEventKick = new EventKick(this);
    pEventKick->setDeadline(nSeconds * 10);
    addEvent(pEventKick);

    // Tell the client how many seconds are left before the kick.
    GCKickMessage gcKickMessage;
    gcKickMessage.setType(KickMessageType);
    gcKickMessage.setSeconds(nSeconds);
    sendPacket(&gcKickMessage);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////
// PaySystem related
//////////////////////////////////////////////////////////////////
bool GamePlayer::loginPayPlay(PayType payType, const string& PayPlayDate, int PayPlayHours, uint payPlayFlag,
                              const string& ip, const string& playerID) {
    __BEGIN_TRY
    return PaySystem::loginPayPlay(payType, PayPlayDate, PayPlayHours, payPlayFlag, ip, playerID);
    __END_CATCH
}

bool GamePlayer::loginPayPlay(const string& ip, const string& playerID) {
    __BEGIN_TRY
    bool bRet = PaySystem::loginPayPlay(ip, playerID);

    if (bRet)
        setPCRoomLottoStartTime();

    return bRet;
    __END_CATCH
}

bool GamePlayer::updatePayPlayTime(const string& playerID, const VSDateTime& currentDateTime,
                                   const Timeval& currentTime) {
    __BEGIN_TRY
    checkPCRoomLotto(currentTime);

    return PaySystem::updatePayPlayTime(playerID, currentDateTime, currentTime);
    __END_CATCH
}

void GamePlayer::logoutPayPlay(const string& playerID, bool bClear, bool bDecreaseTime) {
    __BEGIN_TRY
    savePCRoomLottoTime();

    PaySystem::logoutPayPlay(playerID, bClear, bDecreaseTime);
    __END_CATCH
}

bool GamePlayer::isPayPlaying() const {
    // Every player counts as a paying player: there is no billing backend.
    return true;
}

void GamePlayer::setPCRoomLottoStartTime() {
    if (!de::gameContext().variables().isPCRoomLottoEvent())
        return;

    if (!m_bPCRoomPlay)
        return;

    m_PCRoomLottoStartTime.tv_sec = m_PayPlayStartTime.tv_sec - m_PCRoomLottoSumTime;
}

void GamePlayer::savePCRoomLottoTime() {
    if (!de::gameContext().variables().isPCRoomLottoEvent())
        return;

    if (!m_bPCRoomPlay)
        return;

    Timeval currentTime;
    getCurrentTime(currentTime);

    m_PCRoomLottoSumTime = currentTime.tv_sec - m_PCRoomLottoStartTime.tv_sec;
    m_PCRoomLottoStartTime.tv_sec = 0;
}

void GamePlayer::checkPCRoomLotto(const Timeval& currentTime) {
    if (!de::gameContext().variables().isPCRoomLottoEvent())
        return;

    if (!m_bPCRoomPlay)
        return;

    if (m_PCRoomLottoStartTime.tv_sec == 0) {
        m_PCRoomLottoStartTime.tv_sec = currentTime.tv_sec - m_PCRoomLottoSumTime;
    }

    int time = currentTime.tv_sec - m_PCRoomLottoStartTime.tv_sec;

    if (time >= PCRoomLottoSec) {
        giveLotto();

        // Reset the time.
        m_PCRoomLottoStartTime.tv_sec = currentTime.tv_sec;
        m_PCRoomLottoSumTime = 0;
    }
}

void GamePlayer::giveLotto() {
    if (m_pCreature == NULL)
        return;

    static uint DimensionID = de::kernelContext().config().getPropertyInt("Dimension");
    static uint WorldID = de::kernelContext().config().getPropertyInt("WorldID");
    string PlayerID = getID();
    string Name = m_pCreature->getName();
    Race_t Race = m_pCreature->getRace();
    int Amount = 0;

    SessionRepository& repository = defaultSessionRepository();

    if (repository.loadPCRoomLottoAmount(PlayerID, Name, DimensionID, WorldID, Amount)) {
        if (Amount < PCRoomLottoMaxAmount) {
            repository.updatePCRoomLottoAmount(Amount + 1, PlayerID, Name, DimensionID, WorldID);
        }
    } else {
        // No lotto row yet: insert one.
        repository.insertPCRoomLotto(m_PCRoomID, PlayerID, DimensionID, WorldID, Name, Race);
    }

    if (Amount < PCRoomLottoMaxAmount) {
        char msg[100];
        snprintf(msg, sizeof(msg), de::gameContext().strings().c_str(STRID_GIVE_LOTTO), Amount + 1);

        GCSystemMessage gcMsg;
        gcMsg.setMessage(msg);
        sendPacket(&gcMsg);

        if (Amount >= PCRoomLottoMaxAmount - 1) {
            gcMsg.setMessage(de::gameContext().strings().getString(STRID_CANNOT_GIVE_LOTTO));
            sendPacket(&gcMsg);
        }
    }
}

bool GamePlayer::startPacketLog(uint sec) {
    if (m_pCreature == NULL)
        return false;

    m_bPacketLog = true;
    getCurrentTime(m_PacketLogEndTime);
    m_PacketLogEndTime.tv_sec += sec;

    char filename[100];
    snprintf(filename, sizeof(filename), "log/%s.log", m_pCreature->getName().c_str());
    m_PacketLogFileName = filename;

    return true;
}

void GamePlayer::logLoginoutDateTime() {
    Properties& config = de::kernelContext().config();

    if (m_pCreature == NULL)
        return;

    // The dimension the row belongs to: the configured one, except on a
    // NetMarble deployment, whose rows all go to dimension 2.
    uint dimensionID = config.getPropertyInt("Dimension");
    if (de::isNetMarbleDeployment())
        dimensionID = 2;

    // WorldID
    uint worldID = config.getPropertyInt("WorldID");

    // Race code
    uint racecode;
    uint str, dex, inte;
    if (m_pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(m_pCreature);
        Assert(pSlayer != NULL);

        racecode = (uint)pSlayer->getHighestSkillDomain();

        str = pSlayer->getSTR();
        dex = pSlayer->getDEX();
        inte = pSlayer->getINT();
    } else if (m_pCreature->isVampire()) {
        // Vampire is 10
        racecode = 10;

        Vampire* pVampire = dynamic_cast<Vampire*>(m_pCreature);
        Assert(pVampire != NULL);

        str = pVampire->getSTR();
        dex = pVampire->getDEX();
        inte = pVampire->getINT();
    } else if (m_pCreature->isOusters()) {
        // Ousters is 20
        racecode = 20;

        Ousters* pOusters = dynamic_cast<Ousters*>(m_pCreature);
        Assert(pOusters != NULL);

        str = pOusters->getSTR();
        dex = pOusters->getDEX();
        inte = pOusters->getINT();
    } else {
        return;
    }

    // Level
    uint level = (uint)m_pCreature->getLevel();

    // Logout time. The current time
    VSDateTime logoutDateTime = VSDateTime::currentDateTime();

    // filename
    char filename[20];
    snprintf(filename, sizeof(filename), "log/%s.txt", logoutDateTime.toStringforWeb().c_str());

    try {
        ofstream file(filename, ios::out | ios::app);
        file << dimensionID << "\t" << worldID << "\t" << m_ID << "\t" << m_pCreature->getName() << "\t" << racecode
             << "\t" << level << "\t" << str << "\t" << dex << "\t" << inte << "\t" << m_LoginDateTime.toDateTime()
             << "\t" << logoutDateTime.toDateTime() << endl;
        file.close();
    } catch (...) {
    }
}
