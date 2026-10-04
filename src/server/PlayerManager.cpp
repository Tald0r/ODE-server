//////////////////////////////////////////////////////////////////////
//
// Filename    : PlayerManager.cc
// Written by  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "PlayerManager.h"

#include <algorithm>
#include <exception>

#include "Assert.h"
#include "Player.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "gameserver/Creature.h"
#include "gameserver/Slayer.h"


//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
PlayerManager::PlayerManager() : m_nPlayers(0) {
    __BEGIN_TRY

    // Initialise the array of player pointers to NULL.
    for (uint i = 0; i < nMaxPlayers; i++)
        m_pPlayers[i] = NULL;

    // Initialise the array of player pointers to NULL.
    for (uint i = 0; i < nMaxPlayers; i++)
        m_pCopyPlayers[i] = NULL;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
PlayerManager::~PlayerManager() noexcept(false) {
    // Destructors must not throw; perform best-effort cleanup.
    try {
        for (uint i = 0; i < nMaxPlayers; i++) {
            if (m_pPlayers[i] != NULL) {
                // If the player object exists, the destructor deletes it.
                delete m_pPlayers[i];
                m_pPlayers[i] = NULL;
            }
        }
    } catch (const std::exception&) {
        // swallow
    }
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void PlayerManager::broadcastPacket(Packet* pPacket) {
    __BEGIN_TRY

    for (uint i = 0; i < nMaxPlayers; i++) {
        if (m_pPlayers[i] != NULL) {
            try {
                m_pPlayers[i]->sendPacket(pPacket);
            } catch (Throwable& t) {
                diagnosticFilelog("playerManager.log", "broadcastPacket: %s", t.toString().c_str());
            }
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Adds a given player to the manager.
//////////////////////////////////////////////////////////////////////
void PlayerManager::addPlayer(Player* pPlayer) {
    __BEGIN_TRY

    SOCKET fd = pPlayer->getSocket()->getSOCKET();

    // bound check
    Assert(fd >= 0 && fd < (SOCKET)nMaxPlayers);

    // null check
    if (m_pPlayers[fd] != NULL) {
        ofstream file("Socket_Error.txt", ios::out | ios::app);
        file << "socket descriptor duplicated" << endl;
        file.close();

        cout << "socket descriptor duplicated" << endl;
        throw DuplicatedException("socket descriptor duplicated");
    }

    // Store the pointer that came in.
    m_pPlayers[fd] = pPlayer;

    // Increase the player count.
    m_nPlayers++;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Deletes a given player from the manager.
// The object itself is not deleted; the pointer is set to NULL.
//////////////////////////////////////////////////////////////////////
void PlayerManager::deletePlayer(SOCKET fd) {
    __BEGIN_TRY

    // bound check
    if (fd < 0 || fd >= (SOCKET)nMaxPlayers)
        throw OutOfBoundException();

    // null check
    if (m_pPlayers[fd] == NULL)
        throw NoSuchElementException();

    // Clear the fd-th slot of the array.
    m_pPlayers[fd] = NULL;

    // Decrease the player count.
    m_nPlayers--;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Gets a given player object.
//////////////////////////////////////////////////////////////////////
Player* PlayerManager::getPlayer(SOCKET fd) {
    __BEGIN_TRY

    // bound check
    if (fd < 0 || fd >= (SOCKET)nMaxPlayers)
        throw OutOfBoundException();

    // null check
    if (m_pPlayers[fd] == NULL)
        throw NoSuchElementException();

    return m_pPlayers[fd];

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
// Copies the players.
//////////////////////////////////////////////////////////////////////
void PlayerManager::copyPlayers() {
    __BEGIN_TRY

    for (uint i = 0; i < nMaxPlayers; i++) {
        m_pCopyPlayers[i] = m_pPlayers[i];
    }

    __END_CATCH
}
