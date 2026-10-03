//////////////////////////////////////////////////////////////////////
//
// Filename    : LoginPlayer.h
// Written by  : reiot@ewestsoft.com
// Description : Player class for the game server
//
//////////////////////////////////////////////////////////////////////

#ifndef __LOGIN_PLAYER_H__
#define __LOGIN_PLAYER_H__

// include files
#include <deque>

#include "GCReconnectLogin.h"
#include "LoginKickCache.h"
#include "Mutex.h"
#include "Packet.h"
#include "PaySystem.h"
#include "Player.h"
#include "PlayerStatus.h"
#include "Timeval.h"

//////////////////////////////////////////////////////////////////////
//
// class LoginPlayer
//
// Player class for the game server
//
// Inherits the Player class and adds the Mutex used only on the game server,
// the Creature-related data and methods and the PreviousPacket-related
// data and methods.
//
// processOutput() and sendPacket() in particular can hit a race condition,
// so they must be protected by a Mutex. (That is the MODE-IV case; for MODE-I
// and MODE-II both processInput() and processCommand() must be protected by a
// Mutex.)
//
//////////////////////////////////////////////////////////////////////

class LoginPlayer : public Player, public PaySystem {
public:
    // Number of previous packets to keep
    static const uint nPacketHistory = 10;

    static const uint maxFailure = 3;

public:
    // constructor
    LoginPlayer(Socket* pSocket);

    // destructor
    ~LoginPlayer() noexcept;


    // parse packet and execute handler for the packet
    virtual void processCommand(bool Option = true);


    // send packet to player's output buffer
    virtual void sendPacket(Packet* packet);

    // disconnect
    // A proper logout is disconnect(LOGOUT)
    virtual void disconnect(bool bDisconnected = DISCONNECTED);
    virtual void disconnect_nolog(bool bDisconnected = DISCONNECTED);


    // get debug string
    virtual string toString() const;

public:
    // return recent N-th packet
    // Return the N-th most recently sent packet.
    Packet* getOldPacket(uint prev = 0);

    // return recent packet which has packetID
    // Return the most recent packet with a given ID.
    Packet* getOldPacket(PacketID_t packetID);

    // get/set player's status
    PlayerStatus getPlayerStatus() const {
        return m_PlayerStatus;
    }
    void setPlayerStatus(PlayerStatus playerStatus) {
        m_PlayerStatus = playerStatus;
    }

    // Number of failures
    uint getFailureCount() const {
        return m_FailureCount;
    }
    void setFailureCount(uint nFailed) {
        m_FailureCount = nFailed;
    }

    // get / set GoreLevel
    bool isAdult() const {
        return m_isAdult;
    }
    void setAdult(bool isAdult) {
        m_isAdult = isAdult;
    }

public:
    int getKickCharacterCount() const {
        return m_KickCharacterCount;
    }
    void setExpireTimeForKickCharacter();

    // ID of the current world
    WorldID_t getWorldID() const {
        return m_WorldID;
    }
    void setWorldID(WorldID_t WorldID) {
        m_WorldID = WorldID;
    }

    // Only the current account can read its saved kick target, including when
    // identity was assigned through Player. Live world/group selection is separate.
    const de::LoginKickTarget* getLoginKickTarget() const noexcept {
        return m_LoginKickCache.find(m_ID);
    }
    void cacheLoginKickTarget(const de::LoginKickTarget& target) {
        m_LoginKickCache.store(m_ID, target);
    }
    void clearLoginKickTarget() noexcept {
        m_LoginKickCache.clear();
    }

    const string& getZipcode() const {
        return m_Zipcode;
    }
    void setZipcode(const string& zipcode) {
        m_Zipcode = zipcode;
    }

    const string& getSSN() const {
        return m_SSN;
    }
    void setSSN(const string& ssn) {
        m_SSN = ssn;
    }

    bool isFreePass() const {
        return m_bFreePass;
    }
    void setFreePass(bool bFreePass = true) {
        m_bFreePass = bFreePass;
    }

    bool isWebLogin() const {
        return m_bWebLogin;
    }
    void setWebLogin(bool bWebLogin = true) {
        m_bWebLogin = bWebLogin;
    }

public:
    void sendLGKickCharacter();
    void sendLCLoginOK();

public:
    void setGameServerIP(const string& ip) {
        m_gameServerIP = ip;
    }
    const string& getGameServerIP() {
        return m_gameServerIP;
    }

private:
    // previous packet queue
    deque<Packet*> m_PacketHistory;

    // player status
    PlayerStatus m_PlayerStatus;

    // expire time
    Timeval m_ExpireTime;

    // Number of failed character registrations (CLRegisterPlayer) or logins (CLLogin)
    uint m_FailureCount;

    // mutex
    mutable Mutex m_Mutex;

    // Current world ID
    WorldID_t m_WorldID;
    de::LoginKickCache m_LoginKickCache;

    // Because of LoginPlayerData.
    string m_SSN;
    string m_Zipcode;

    bool m_isAdult;

    // Time to wait for the forced disconnect in the 'already connected' case
    uint m_KickCharacterCount;
    Timeval m_ExpireTimeForKickCharacter;

    // Treated as verified elsewhere (Netmarble), so a FreePass is granted.
    bool m_bFreePass;

    // Web login mode
    bool m_bWebLogin;

    // m_gameServerIP is set in CLSelectPCHandler.
    string m_gameServerIP;
};

#endif
