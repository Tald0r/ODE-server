//////////////////////////////////////////////////////////////////////////////
// Filename    : GamePlayer.h
// Written by  : reiot@ewestsoft.com
// Description : player class for the game server
//////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include <bitset>
#include <deque>

#include "EventManager.h"
#include "Exception.h"
#include "GCReconnectLogin.h"
#include "GameFrameGate.h"
#include "Mutex.h"
#include "Packet.h"
#include "PaySystem.h"
#include "Player.h"
#include "PlayerMailbox.h"
#include "PlayerStatus.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"
#include "SpeedHackDecision.h"
#include "Timeval.h"
#include "Types.h"
#include "skill/Skill.h"

// #include "gameguard/CSAuth.h"

//////////////////////////////////////////////////////////////////////////////
// class GamePlayer
//
// Player class for the game server.
//
// It derives from Player and adds the Mutex, the Creature-related data and
// methods, and the PreviousPacket data and methods, all of which are used
// only by the game server.
//
// processOutput() and sendPacket() in particular can race, so they must be
// protected by the Mutex. (That is the MODE-IV case; under MODE-I and
// MODE-II, processInput() and processCommand() both have to be protected
// by the Mutex as well.)
//////////////////////////////////////////////////////////////////////////////

class Creature;

class GamePlayer : public Player, public PaySystem {
public:
    // Number of previous packets kept.
    const static BYTE nPacketHistorySize = 10;

public:
    GamePlayer(Socket* pSocket);
    ~GamePlayer() noexcept;

public:
    // read socket's receive buffer and fill input buffer
    // virtual void processInput() ;

    // parse packet and execute handler for the packet
    virtual void processCommand(bool Option = true);

    // flush output buffer to socket's send buffer
    virtual void processOutput();

    // send packet to player's output buffer
    virtual void sendPacket(Packet* packet);

    // disconnect
    // A normal logout calls disconnect(LOGOUT).
    virtual void disconnect(bool bDisconnected = DISCONNECTED);

    // get debug string
    virtual string toString() const;

    // Speed check: is the client heartbeat that just arrived plausible?
    bool verifySpeed();

    // get creature pointer
    Creature* getCreature() {
        return m_pCreature;
    }
    const Creature* getCreature() const {
        return m_pCreature;
    }

    // set creature pointer
    void setCreature(Creature* pCreature) {
        m_pCreature = pCreature;
    }

    // Commands other threads posted for this player (PlayerMailbox.h);
    // drained by whichever manager owns the player, abandoned by the
    // destructor. Thread-safe to post to from anywhere.
    de::PlayerCommandMailbox& mailbox() {
        return m_Mailbox;
    }

    // return recent N-th packet
    // Return the N-th most recent packet.
    Packet* getOldPacket(uint prev = 0);

    // return recent packet which has packetID
    // Return the most recent packet carrying the given ID.
    Packet* getOldPacket(PacketID_t packetID);

    // get player's status
    PlayerStatus getPlayerStatus() const {
        return m_PlayerStatus;
    }

    // set player's status
    void setPlayerStatus(PlayerStatus playerStatus) {
        m_PlayerStatus = playerStatus;
    }

    //
    void addEvent(Event* pEvent);
    Event* getEvent(Event::EventClass EClass);
    void deleteEvent(Event::EventClass EClass);

    // Penalty status functions
    // Set Flag
    void setPenaltyFlag(PenaltyType PenaltyFlag) {
        m_PenaltyFlag.set(PenaltyFlag);
    }

    // remove Flag
    void removePenaltyFlag(PenaltyType PenaltyFlag) {
        m_PenaltyFlag.reset(PenaltyFlag);
    }

    // Is Flag?
    bool isPenaltyFlag(PenaltyType PenaltyFlag) {
        return m_PenaltyFlag.test(PenaltyFlag);
    }

public:
    uint getSpecialEventCount(void) const {
        return m_SpecialEventCount;
    }
    void setSpecialEventCount(uint count) {
        m_SpecialEventCount = count;
    }
    void loadSpecialEventCount(void);
    void saveSpecialEventCount(void);

public: // Forced disconnect for the 'already connected' case.
    bool isKickForLogin() const {
        return m_bKickForLogin;
    }
    void setKickForLogin(bool bKickForLogin = true) {
        m_bKickForLogin = bKickForLogin;
    }

    const string& getKickRequestHost() const {
        return m_KickRequestHost;
    }
    uint getKickRequestPort() const {
        return m_KickRequestPort;
    }

    void setKickRequestHost(const string& host) {
        m_KickRequestHost = host;
    }
    void setKickRequestPort(uint port) {
        m_KickRequestPort = port;
    }

public:
    void setReconnectPacket(GCReconnectLogin* pPacket) {
        SAFE_DELETE(m_pReconnectPacket);
        m_pReconnectPacket = pPacket;
    }
    GCReconnectLogin* getReconnectPacket() const {
        return m_pReconnectPacket;
    }

    // by sigi. 2002.10.23
    bool isFreePass() const {
        return m_bFreePass;
    }
    void setFreePass(bool bFreePass = true) {
        m_bFreePass = bFreePass;
    }

public:
    void lock() {
        m_Mutex.lock();
    }
    void unlock() {
        m_Mutex.unlock();
    }

public:
    // Packet encryption
    // by sigi. 2002.11.27
    void setEncryptCode();

public:
    void kickPlayer(uint nSeconds, uint KickMessageType);

    //////////////////////////////////////////////////
    // PaySystem
    //////////////////////////////////////////////////
public:
    bool loginPayPlay(PayType payType, const string& PayPlayDate, int PayPlayHours, uint payPlayFlag, const string& ip,
                      const string& playerID);

    bool loginPayPlay(const string& ip, const string& playerID);

    bool updatePayPlayTime(const string& playerID, const VSDateTime& currentDateTime, const Timeval& currentTime);

    void logoutPayPlay(const string& playerID, bool bClear = false, bool bDecreaseTime = true);

    bool isPayPlaying() const;

    int getItemRatioBonusPoint(void) const {
        return m_ItemRatioBonusPoint;
    }
    void setItemRatioBonusPoint(int point) {
        m_ItemRatioBonusPoint = point;
    }

    bool startPacketLog(uint sec);
    // CCSAuth&	getCSAuth() { return m_NProtectCSAuth; }

    void logLoginoutDateTime();


private:
    void setPCRoomLottoStartTime();
    void checkPCRoomLotto(const Timeval& currentTime);
    void savePCRoomLottoTime();
    void giveLotto();

private:
    // creature
    Creature* m_pCreature;

    de::PlayerCommandMailbox m_Mailbox;

    // previous packet queue
    deque<Packet*> m_PacketHistory;

    // player status
    PlayerStatus m_PlayerStatus;

    // Panelty status
    bitset<PENALTY_TYPE_MAX> m_PenaltyFlag;

    // expire time
    Timeval m_ExpireTime;

    // How the client heartbeat check stands for this session.
    de::HeartbeatVerifyState m_SpeedVerify;

    // mutex
    mutable Mutex m_Mutex;

    EventManager m_EventManager;

    // Counter for special events.
    // Usable for events such as collecting lots of skulls.
    uint m_SpecialEventCount;

    // Set when the 'already connected' case forces a disconnect.
    bool m_bKickForLogin;
    string m_KickRequestHost;
    uint m_KickRequestPort;

    // Used when going from the GameServer to the LoginServer.
    GCReconnectLogin* m_pReconnectPacket;

    bool m_bFreePass;

    // Per-player bonus chance for acquiring items.
    int m_ItemRatioBonusPoint;

    Timeval m_PCRoomLottoStartTime; // PC room lottery: time the calculation started
    uint m_PCRoomLottoSumTime;      // PC room lottery: accumulated time, saved on logoutPayPlay

    string m_PacketLogFileName;
    bool m_bPacketLog;
    Timeval m_PacketLogEndTime;

    // CCSAuth			m_NProtectCSAuth;

    VSDateTime m_LoginDateTime;


    // Logs why the frame gate refused a frame and throws the exception that
    // drops the connection for it.
    [[noreturn]] void refuseFrame(const de::GameFrame& frame);

    // Decides each frame the client sends and counts its sequence bytes.
    de::GameFrameGate m_FrameGate;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
class isSamePlayer {
public:
    isSamePlayer(GamePlayer* pGamePlayer) : m_pGamePlayer(pGamePlayer) {}
    bool operator()(GamePlayer* pGamePlayer) {
        return pGamePlayer->getID() == m_pGamePlayer->getID();
    }

private:
    // Creature Pointer
    GamePlayer* m_pGamePlayer;
};

#endif
