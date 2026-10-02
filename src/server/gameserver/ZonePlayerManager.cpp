//
// Filename    : ZonePlayerManager.cpp
// Written by  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ZonePlayerManager.h"

#include <stdio.h>

#include <algorithm>
#include <mutex>

#include "Assert.h"
#include "CGLogout.h"
#include "DescriptorTable.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GamePlayerHandoff.h"
#include "IncomingPlayerManager.h"
#include "PaySystem.h"
#include "PlayerCreature.h"
#include "PlayerMailbox.h"
#include "Profile.h"
#include "ResurrectLocationManager.h"
#include "Slayer.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "StringStream.h"
#include "Thread.h"
#include "Zone.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"
// #include "UserGateway.h"
#include "BroadcastFilter.h"
#include "DefaultOptionSetInfo.h"
#include "GCKickMessage.h"
#include "GCSystemMessage.h"
#include "Party.h"
#include "StringPool.h"
#include "VariableManager.h"
#include "ZoneGroup.h"
#include "repository/MessageRepository.h"


#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif

bool checkZonePlayerManager(GamePlayer* pGamePlayer, ZonePlayerManager* pZPM, const string& str);

//////////////////////////////////////////////////////////////////////////////
// constructor
// Create the sub-manager objects.
//////////////////////////////////////////////////////////////////////////////
ZonePlayerManager::ZonePlayerManager()

    : m_PollSet((int)nMaxPlayers), m_TimeoutMilliseconds(0), m_MinFD(-1), m_MaxFD(-1) {
    __BEGIN_TRY

    m_Mutex.setName("ZonePlayerManager");
    m_MutexBroadcast.setName("ZonePlayerManagerBroadcast");
    m_PlayerListQueue.clear();
    m_BroadcastQueue.clear();

    // How long a poll waits. This interval should eventually become a
    // configuration option.
    m_TimeoutMilliseconds = 0;
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
ZonePlayerManager::~ZonePlayerManager() noexcept {
    releasePlayers(false);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::broadcastPacket(Packet* pPacket)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    PlayerManager::broadcastPacket(pPacket);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::broadcastPacket_NOBLOCKED(Packet* pPacket)

{
    __BEGIN_TRY

    PlayerManager::broadcastPacket(pPacket);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::pushBroadcastPacket(Packet* pPacket, BroadcastFilter* pFilter)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexBroadcast)

    // This assumes the packets written here do not use an Encrypter.
    // To use packets that need an Encrypter, the BroadcastQueue would have to live
    // in the Zone and be used from there.

    // Put the filter and the packet into the queue.
    // The filter is stored as a newly created clone.
    // The packet is written to a stream and the stream is queued.
    SocketOutputStream* pStream = new SocketOutputStream(NULL, szPacketHeader + pPacket->getPacketSize());
    pPacket->writeHeaderNBody(*pStream);

    m_BroadcastQueue.push_back(PairFilterStream(pFilter->Clone(), pStream));

    __LEAVE_CRITICAL_SECTION(m_MutexBroadcast)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::flushBroadcastPacket()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_MutexBroadcast)

    list<PairFilterStream>::iterator itr = m_BroadcastQueue.begin();
    list<PairFilterStream>::iterator endItr = m_BroadcastQueue.end();

    for (; itr != endItr; ++itr) {
        BroadcastFilter* pFilter = itr->first;
        SocketOutputStream* pStream = itr->second;

        if (pStream == NULL) {
            filelog("ZoneBug.txt", "%s : %s", "Zone::flushBroadcastPacket", "pStream is NULL.");
            continue;
        }

        for (uint i = 0; i < nMaxPlayers; ++i) {
            if (m_pPlayers[i] != NULL) {
                GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                if (pFilter == NULL || pFilter->isSatisfy(pGamePlayer)) {
                    try {
                        pGamePlayer->sendStream(pStream);
                    } catch (Throwable& t) {
                        filelog("ZonePlayerManager.log", "broadcastPacket: %s", t.toString().c_str());
                    }
                }
            }
        }
    }

    m_BroadcastQueue.clear();

    __LEAVE_CRITICAL_SECTION(m_MutexBroadcast)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::copyPlayers()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    PlayerManager::copyPlayers();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Ask the kernel which descriptors are ready.
// When none is, there are no players to process.
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::pollSockets() {
    __BEGIN_TRY

    // The membership is read and the answers are stored under the mutex that
    // guards it, while the wait itself runs without it: other threads add and
    // remove players here.
    __ENTER_CRITICAL_SECTION(m_Mutex)

    m_PollSet.fill();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // A failed wait, which is what an arriving signal makes of it, leaves
    // every descriptor unready, so this tick processes nothing and the next
    // one asks again.
    m_PollSet.wait(m_TimeoutMilliseconds);

    __ENTER_CRITICAL_SECTION(m_Mutex)

    // A player added or removed while the wait ran keeps no answer from it.
    m_PollSet.collect();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// process all players' inputs
//
// When the server socket's read flag is set a new connection has arrived and is
// handled here; when another socket's read flag is set a new packet has arrived,
// so that player's processInput() is called.
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::processInputs() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        // The ZPM holds only players, so there is nothing further to compare.
        if (m_PollSet.isReadable(i)) {
            if (m_pPlayers[i] != NULL) {
                GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                Assert(pTempPlayer != NULL);
                Assert(m_pPlayers[i] != NULL);

                if (de::gameContext().variables().getVariable(PCROOM_ITEM_RATIO_BONUS) == 100 &&
                    !checkZonePlayerManager(pTempPlayer, this, "PI")) {
                    try {
                        CGLogoutHandler::execute(NULL, pTempPlayer);
                    } catch (DisconnectException& de) {
                        moveToOutgoing(pTempPlayer);
                    }
                } else if (pTempPlayer->getSocket()->getSockError()) {
                    pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                    pTempPlayer->setItemRatioBonusPoint(7);

                    try {
                        CGLogoutHandler::execute(NULL, pTempPlayer);
                    } catch (DisconnectException& de) {
                        filelog("DIFF_ZG.log", "%s ZPM+PI+SOCKERR", de.toString().c_str());
                        moveToOutgoing(pTempPlayer);
                    }


                } else {
                    try {
                        pTempPlayer->processInput();
                    } catch (ConnectException& ce) {
                        pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                        pTempPlayer->setItemRatioBonusPoint(8);

                        try {
                            CGLogoutHandler::execute(NULL, pTempPlayer);
                        } catch (DisconnectException& de) {
                            filelog("DIFF_ZG.log", "%s ZPM+PI+CE", de.toString().c_str());
                            moveToOutgoing(pTempPlayer);
                        }


                    } catch (IOException& ioe) {
                        pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                        pTempPlayer->setItemRatioBonusPoint(9);

                        try {
                            CGLogoutHandler::execute(NULL, pTempPlayer);
                        } catch (DisconnectException& de) {
                            filelog("DIFF_ZG.log", "%s ZPM+PI+IOE", de.toString().c_str());
                            moveToOutgoing(pTempPlayer);
                        }
                    }
                }
            }
        }
    }


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// process all players' commands
//////////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::processCommands() {
    __BEGIN_TRY
    __BEGIN_DEBUG

    // test code

    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    VSDateTime currentDateTime(VSDate::currentDate(), VSTime::currentTime());

    Timeval currentTime;
    getCurrentTime(currentTime);

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_pPlayers[i] != NULL) {
            GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
            Assert(pTempPlayer);
            Assert(m_pPlayers[i] != NULL);

            if (pTempPlayer->getSocket()->getSockError()) {
                pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                pTempPlayer->setItemRatioBonusPoint(10);

                try {
                    CGLogoutHandler::execute(NULL, pTempPlayer);
                } catch (DisconnectException& de) {
                    filelog("DIFF_ZG.log", "%s ZPM+PC+SOCKERR", de.toString().c_str());
                    moveToOutgoing(pTempPlayer);
                }


            } else {
                bool IsPayPlayEnd = false;

                // This manager owns pTempPlayer and the group mutex is held:
                // run what other threads posted for it (PlayerMailbox.h)
                // before its own packets.
                de::drainPlayerMailbox(*pTempPlayer, *this);

                try {
                    beginProfileEx("ZPM_PACKET");
                    pTempPlayer->processCommand();
                    endProfileEx("ZPM_PACKET");

                    if (de::gameContext().variables().getVariable(PCROOM_ITEM_RATIO_BONUS) == 100 &&
                        !checkZonePlayerManager(pTempPlayer, this, "PC")) {
                        try {
                            CGLogoutHandler::execute(NULL, pTempPlayer);
                        } catch (DisconnectException& de) {
                            moveToOutgoing(pTempPlayer);
                        }
                    }

                    // Only check pay status in the normal game state (GPS_NORMAL).
                    // PCManager::killCreature() switches to GPS_IGNORE_ALL, removes the creature from
                    // its tile and moves it between zones, which could otherwise cause trouble here.
                    // by sigi. 2002.12.10
                    else if (pTempPlayer->getPlayerStatus() == GPS_NORMAL) {
                        // When the family rate has expired, free party members in a pay zone move to a free zone.
                        if (pTempPlayer->isFamilyFreePassEnd()) {
                            Creature* pCreature = pTempPlayer->getCreature();
                            Zone* pZone = pCreature->getZone();
                            Assert(pZone != NULL);

                            if (pZone->isPayPlay()) {
                                // For a free user the if below does the pay check and moves them to a free zone.
                                pTempPlayer->setPremiumPlay();
                            }
                        }

                        // For a paying user, decrease the remaining time.
                        // For a family-rate user, check whether the time ran out, in pay and free zones alike.
                        if ((pTempPlayer->isPayPlaying() || pTempPlayer->isPremiumPlay() ||
                             pTempPlayer->isFamilyPayAvailable()) &&
                            !pTempPlayer->updatePayPlayTime(pTempPlayer->getID(), currentDateTime, currentTime)) {
                            Creature* pCreature = pTempPlayer->getCreature();
                            Zone* pZone = pCreature->getZone();
                            Assert(pZone != NULL);

                            // End the pay service.
                            pTempPlayer->logoutPayPlay(pTempPlayer->getID());

                            // For a family-rate user, clear the FamilyPayAvailable flag
                            // and drop the family-rate default option.
                            if (pTempPlayer->isFamilyPayAvailable()) {
                                pTempPlayer->setFamilyPayAvailable(false);

                                // If the player is in a party, refresh the family pay state.
                                int PartyID = pCreature->getPartyID();
                                if (PartyID != 0) {
                                    de::gameContext().parties().refreshFamilyPay(PartyID);
                                }

                                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                                Assert(pPC != NULL);

                                pPC->removeDefaultOptionSet(DEFAULT_OPTION_SET_FAMILY_PAY);
                                pPC->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
                            }

                            // by sigi. 2002.12.30
                            IsPayPlayEnd = true;
                        }

                        // Once the family rate has expired, change the type so it is not checked again.
                        if (pTempPlayer->isFamilyFreePassEnd()) {
                            pTempPlayer->setFamilyPayPartyType(FAMILY_PAY_PARTY_TYPE_NONE);
                        }
                    }
                } catch (ProtocolException& pe) {
                    pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                    pTempPlayer->setItemRatioBonusPoint(11);

                    try {
                        CGLogoutHandler::execute(NULL, pTempPlayer);
                    } catch (DisconnectException& de) {
                        filelog("DIFF_ZG.log", "%s ZPM+PC+PE", de.toString().c_str());
                        moveToOutgoing(pTempPlayer);
                    }

                    // by sigi. 2002.12.30
                    if (IsPayPlayEnd) {
                    } else {
                    }
                }
            }
        }
    }


    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// process all players' outputs
//
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::processOutputs() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }


    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isWritable(i)) {
            if (m_pPlayers[i] != NULL) {
                GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                Assert(pTempPlayer);
                Assert(m_pPlayers[i] != NULL);

                if (pTempPlayer->getSocket()->getSockError()) {
                    pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                    pTempPlayer->setItemRatioBonusPoint(12);

                    try {
                        CGLogoutHandler::execute(NULL, pTempPlayer);
                    } catch (DisconnectException& de) {
                        filelog("DIFF_ZG.log", "%s ZPM+PO+SOCKERR", de.toString().c_str());
                        moveToOutgoing(pTempPlayer);
                    }

                } else {
                    try {
                        pTempPlayer->processOutput();
                    } catch (ConnectException& ce) {
                        pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                        pTempPlayer->setItemRatioBonusPoint(13);

                        try {
                            CGLogoutHandler::execute(NULL, pTempPlayer);
                        } catch (DisconnectException& de) {
                            filelog("DIFF_ZG.log", "%s ZPM+PO+CE", de.toString().c_str());
                            moveToOutgoing(pTempPlayer);
                        }


                    } catch (ProtocolException& cp) {
                        pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                        pTempPlayer->setItemRatioBonusPoint(14);

                        try {
                            CGLogoutHandler::execute(NULL, pTempPlayer);
                        } catch (DisconnectException& de) {
                            filelog("DIFF_ZG.log", "%s ZPM+PO+PE", de.toString().c_str());
                            moveToOutgoing(pTempPlayer);
                        }
                    }
                }
            }
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// process all players' exceptions
//
// There is no plan to send OOB data.
// So if OOB is set it is treated as an error and the connection is cut.
//
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::processExceptions() {
    __BEGIN_TRY


    if (m_MinFD == -1 && m_MaxFD == -1) // no player exist
    {
        return;
    }

    const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
    for (int i = walk.first; i <= walk.last; i++) {
        if (m_PollSet.isUrgent(i)) {
            if (m_pPlayers[i] != NULL) {
                GamePlayer* pTempPlayer = dynamic_cast<GamePlayer*>(m_pPlayers[i]);
                Assert(pTempPlayer != NULL);
                Assert(m_pPlayers[i] != NULL);

                pTempPlayer->setPenaltyFlag(PENALTY_TYPE_KICKED);
                pTempPlayer->setItemRatioBonusPoint(15);

                try {
                    CGLogoutHandler::execute(NULL, pTempPlayer);
                } catch (DisconnectException& de) {
                    filelog("DIFF_ZG.log", "%s ZPM+PE", de.toString().c_str());
                    moveToOutgoing(pTempPlayer);
                }
            }
        }
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Add a player to the manager.
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::addPlayer(GamePlayer* pGamePlayer) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    PlayerManager::addPlayer(pGamePlayer);


    SOCKET fd = pGamePlayer->getSocket()->getSOCKET();

    // Readjust m_MinFD and m_MaxFD.
    if (m_MinFD == -1 && m_MaxFD == -1) {
        // The first player.
        m_MinFD = m_MaxFD = fd;
    } else {
        m_MinFD = min(fd, m_MinFD);
        m_MaxFD = max(fd, m_MaxFD);
    }

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Add a player to the manager.
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::addPlayer_NOBLOCKED(GamePlayer* pGamePlayer) {
    __BEGIN_TRY

    PlayerManager::addPlayer(pGamePlayer);


    SOCKET fd = pGamePlayer->getSocket()->getSOCKET();

    // Readjust m_MinFD and m_MaxFD.
    if (m_MinFD == -1 && m_MaxFD == -1) {
        // The first player.
        m_MinFD = m_MaxFD = fd;
    } else {
        m_MinFD = min(fd, m_MinFD);
        m_MaxFD = max(fd, m_MaxFD);
    }

    // Watch the new descriptor. It is reported ready no earlier than the next
    // poll.
    m_PollSet.watch(fd, de::DescriptorPollSet::kRead | de::DescriptorPollSet::kWrite | de::DescriptorPollSet::kUrgent);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Remove a player from the manager.
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::deletePlayer_NOBLOCKED(SOCKET fd) {
    __BEGIN_TRY

    // Remove the player pointer from the player array.
    PlayerManager::deletePlayer(fd);

    Assert(m_pPlayers[fd] == NULL);

    // Readjust m_MinFD and m_MaxFD.
    // The fd == m_MinFD && fd == m_MaxFD case is handled by the first if.
    if (fd == m_MinFD) {
        // Search forward for the smallest fd.
        // Note that the m_MinFD slot is now NULL.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.first;
        for (i = walk.first; i <= walk.last; i++) {
            if (m_pPlayers[i] != NULL) {
                m_MinFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        // this is the m_MinFD == m_MaxFD case.
        // Set both to -1.
        if (i > walk.last)
            m_MinFD = m_MaxFD = -1;

    } else if (fd == m_MaxFD) {
        // Search backward for the largest fd.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.last;
        for (i = walk.last; i >= walk.first; i--) {
            if (m_pPlayers[i] != NULL) {
                m_MaxFD = i;
                break;
            }
        }

        // When no suitable m_MaxFD was found,
        if (i < walk.first) {
            filelog("ZonePlayerManagerBug.txt", "%s : %s", "ZonePlayerManager::deletePlayer_NOBLOCKED()",
                    "MinMaxFD problem");
            throw UnknownError("m_MinFD & m_MaxFD problem.");
        }
    }

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because later processing could otherwise still service
    // an object that is already gone.
    m_PollSet.unwatch(fd);


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Remove a player from the manager.
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::deletePlayer(SOCKET fd) {
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    try {
        // Remove the player pointer from the player array.
        PlayerManager::deletePlayer(fd);
    } catch (OutOfBoundException& o) {
        filelog("ZPMError.txt", "OOB: %s, Socket: %d", o.toString().c_str(), fd);
        throw;
    } catch (NoSuchElementException& n) {
        filelog("ZPMError.txt", "NSEE: %s, Socket: %d", n.toString().c_str(), fd);
        throw;
    } catch (Error& e) {
        filelog("ZPMError.txt", "Error: %s, Socket: %d", e.toString().c_str(), fd);
        throw;
    } catch (...) {
        filelog("ZPMError.txt", "Unknown exception. Socket: %d", fd);
        throw;
    }

    Assert(m_pPlayers[fd] == NULL);

    // Readjust m_MinFD and m_MaxFD.
    // The fd == m_MinFD && fd == m_MaxFD case is handled by the first if.
    if (fd == m_MinFD) {
        // Search forward for the smallest fd.
        // Note that the m_MinFD slot is now NULL.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.first;
        for (i = walk.first; i <= walk.last; i++) {
            if (m_pPlayers[i] != NULL) {
                m_MinFD = i;
                break;
            }
        }

        // When no suitable m_MinFD was found,
        // this is the m_MinFD == m_MaxFD case.
        // Set both to -1.
        if (i > walk.last)
            m_MinFD = m_MaxFD = -1;

    } else if (fd == m_MaxFD) {
        // Search backward for the largest fd.
        const de::DescriptorRange walk = de::descriptorRange((int)m_MinFD, (int)m_MaxFD, (int)nMaxPlayers);
        int i = walk.last;
        for (i = walk.last; i >= walk.first; i--) {
            if (m_pPlayers[i] != NULL) {
                m_MaxFD = i;
                break;
            }
        }

        // When no suitable m_MaxFD was found,
        if (i < walk.first) {
            filelog("ZonePlayerManagerBug.txt", "%s : %s", "ZonePlayerManager::deletePlayer()", "MinMaxFD problem");
            throw UnknownError("m_MinFD & m_MaxFD problem.");
        }
    }

    // Stop watching the descriptor. This also drops the readiness the last poll
    // reported for it, because later processing could otherwise still service
    // an object that is already gone.
    m_PollSet.unwatch(fd);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
Player* ZonePlayerManager::getPlayer(SOCKET fd) {
    __BEGIN_TRY

    Player* pPlayer = NULL;

    __ENTER_CRITICAL_SECTION(m_Mutex)

    pPlayer = PlayerManager::getPlayer(fd);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    return pPlayer;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
Player* ZonePlayerManager::getPlayerByPhoneNumber(PhoneNumber_t PhoneNumber) {
    __BEGIN_TRY
    return NULL;
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Routine that saves every player.
// Another thread can reach it, so it would need a lock here,
// but a save can run while this thread processes events, so
// the lock is taken in the lower-level save and this routine only calls it.
// ZonePlayerManager:: save()
//////////////////////////////////////////////////////////////////////
void ZonePlayerManager::save()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unsigned int i;

    for (i = 0; i < nMaxPlayers; i++) {
        if (m_pPlayers[i] != NULL) {
            Assert(m_pPlayers[i] != NULL); // by sigi

            Creature* pCreature = ((GamePlayer*)m_pPlayers[i])->getCreature();

            Assert(pCreature != NULL); // by sigi

            pCreature->save();
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void ZonePlayerManager::pushPlayer(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pGamePlayer != nullptr);
    m_PlayerListQueue.push_back(pGamePlayer);

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

void ZonePlayerManager::pushOutPlayer(GamePlayer* pGamePlayer)

{
    __BEGIN_TRY

    Assert(pGamePlayer != nullptr);
    m_PlayerOutListQueue.push_back(pGamePlayer);

    __END_CATCH
}

void ZonePlayerManager::moveToOutgoing(GamePlayer* player) {
    std::lock_guard tableLock(m_Mutex);
    Assert(player != nullptr);
    const auto descriptor = de::enqueueRegisteredGamePlayer(m_pPlayers, m_PlayerOutListQueue, *player);
    --m_nPlayers;
    m_PollSet.unwatch(descriptor);
    const auto range = de::gamePlayerDescriptorRange(m_pPlayers);
    m_MinFD = range.empty() ? -1 : range.first;
    m_MaxFD = range.empty() ? -1 : range.last;
}

void ZonePlayerManager::processPlayerListQueue()

{
    __BEGIN_TRY

    // Add the players in the queue to the manager.
    while (!m_PlayerListQueue.empty()) {
        GamePlayer* pGamePlayer = m_PlayerListQueue.front();

        if (pGamePlayer == NULL) {
            m_PlayerListQueue.pop_front();
            filelog("ZoneBug.txt", "%s : %s", "Zone::heartbeat(1)", "pGamePlayer is NULL.");
            continue;
        }

        de::transferFirstGamePlayer(m_PlayerListQueue, [&](GamePlayer* player) { addPlayer_NOBLOCKED(player); });

        Creature* pCreature = pGamePlayer->getCreature();

        Assert(pCreature != NULL);

        // The zone the player is entering.
        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);


        pZone->addPC(pCreature, pCreature->getX(), pCreature->getY(), DOWN);
    }

    __END_CATCH
}

void ZonePlayerManager::heartbeat()

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    processPlayerListQueue();

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    // Process the players waiting to leave.
    // They are simply pushed to the IPM.
    while (!m_PlayerOutListQueue.empty()) {
        de::transferFirstGamePlayer(m_PlayerOutListQueue,
                                    [](GamePlayer* player) { de::gameContext().incomingPlayers().pushPlayer(player); });
    }

    // Process the broadcast packet queue.
    if (!m_BroadcastQueue.empty())
        flushBroadcastPacket();

    __END_CATCH
}

void ZonePlayerManager::deleteQueuePlayer(GamePlayer* pGamePlayer) {
    __BEGIN_TRY

    // No lock is needed here.

    Assert(pGamePlayer != NULL);

    list<GamePlayer*>::iterator itr =
        find_if(m_PlayerOutListQueue.begin(), m_PlayerOutListQueue.end(), isSamePlayer(pGamePlayer));

    if (itr != m_PlayerOutListQueue.end()) {
        m_PlayerOutListQueue.erase(itr);
    }


    __END_CATCH
}

void ZonePlayerManager::removeFlag(Effect::EffectClass EC)

{
    __BEGIN_TRY

    __ENTER_CRITICAL_SECTION(m_Mutex)

    for (uint i = 0; i < nMaxPlayers; i++) {
        Player* pPlayer = m_pPlayers[i];

        if (pPlayer != NULL) {
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
            Assert(pGamePlayer != NULL);

            Creature* pCreature = pGamePlayer->getCreature();
            Assert(pCreature != NULL);

            pCreature->removeFlag(EC);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////
// Clean up every player held by the ZonePlayerManager.
////////////////////////////////////////////////////////////////////////
void ZonePlayerManager::clearPlayers() {
    releasePlayers(true);
}

void ZonePlayerManager::releasePlayers(bool disconnect) noexcept {
    de::releaseGamePlayers(m_pPlayers, m_PlayerListQueue, m_PlayerOutListQueue, m_PollSet, disconnect);
    std::fill(std::begin(m_pCopyPlayers), std::end(m_pCopyPlayers), nullptr);
    m_nPlayers = 0;
    m_MinFD = m_MaxFD = -1;
}

bool checkZonePlayerManager(GamePlayer* pGamePlayer, ZonePlayerManager* pZPM, const string& str) {
    if (pGamePlayer == NULL)
        return true;

    Creature* pCreature = pGamePlayer->getCreature();
    if (pCreature == NULL)
        return true;

    Zone* pZone = pCreature->getZone();
    if (pZone == NULL)
        return true;

    ZoneGroup* pZoneGroup = pZone->getZoneGroup();
    if (pZoneGroup == NULL)
        return true;

    ZonePlayerManager* pZonePlayerManager = pZoneGroup->getZonePlayerManager();
    if (pZonePlayerManager == NULL)
        return true;

    if (pZPM != pZonePlayerManager) {
        filelog("ZPMCheck.log", "CZPM:%u GZPM:%u SOCK:%d ZID:%u NAME:%s P:%s", pZPM->getZGID(),
                pZonePlayerManager->getZGID(), pGamePlayer->getSocket()->getSOCKET(), pZone->getZoneID(),
                pCreature->getName().c_str(), str.c_str());

        return false;
    }

    return true;
}
