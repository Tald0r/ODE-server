//----------------------------------------------------------------------
//
// Filename    : PacketValidator.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "PacketValidator.h"

#if defined(__GAME_SERVER__)
#include "GameClientLink.h"
#endif

//----------------------------------------------------------------------
// constructor
//----------------------------------------------------------------------
PacketValidator::PacketValidator() {
    __BEGIN_TRY

    // One slot per player status, each empty until init() fills it.
    m_PacketIDSets.assign(PLAYER_STATUS_MAX, nullptr);

    __END_CATCH
}

//----------------------------------------------------------------------
// destructor
//----------------------------------------------------------------------
PacketValidator::~PacketValidator() noexcept {
    for (uint i = 0; i < PLAYER_STATUS_MAX; i++) {
        SAFE_DELETE(m_PacketIDSets[i]);
    }

    m_PacketIDSets.clear();
}

//----------------------------------------------------------------------
// initialize packet validators
//----------------------------------------------------------------------
void PacketValidator::init() {
    __BEGIN_TRY

#if defined(__GAME_CLIENT__) || defined(__LOGIN_SERVER__) || defined(__GAME_SERVER__)
    PacketIDSet* pPacketIDSet = NULL;
#endif

    __BEGIN_DEBUG

#if defined(__GAME_CLIENT__)

    //----------------------------------------------------------------------
    // CPS_NONE
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_NONE, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_BEGIN_SESSION ( before the login packet )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_BEGIN_SESSION, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_LOGIN ( right after CLLogin )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_LOGIN);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_VERSION_CHECK_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_VERSION_CHECK_ERROR);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_LOGIN_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_LOGIN_ERROR);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID ( right after CLQueryPlayerID )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_QUERY_RESULT_PLAYER_ID);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_REGISTER_PLAYER ( right after CLRegisterPlayer )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_REGISTER_PLAYER);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_VERSION_CHECK_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_VERSION_CHECK_ERROR);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_REGISTER_PLAYER_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_REGISTER_PLAYER_ERROR);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_GET_PC_LIST ( right after CLGetPCList )
    // This state is kept after CLChangeServer as well.
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_GET_PC_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_PC_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_SERVER_LIST);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_CREATE_PC ( right after CLCreatePC )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_CREATE_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_CREATE_PC_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_CREATE_PC_ERROR);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_DELETE_PC ( right after CLDeletePC )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_DELETE_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_DELETE_PC_OK);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_DELETE_PC_ERROR);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CL_SELECT_PC ( right after CLSelectPC )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CL_SELECT_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_RECONNECT);
    pPacketIDSet->addPacketID(Packet::PACKET_LC_SELECT_PC_ERROR);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_AFTER_SENDING_CG_CONNECT
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_AFTER_SENDING_CG_CONNECT);
    pPacketIDSet->addPacketID(Packet::PACKET_GC_UPDATE_INFO);
    pPacketIDSet->addPacketID(Packet::PACKET_GC_DISCONNECT);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_WAITING_FOR_LOADING
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_WAITING_FOR_LOADING, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_WAITING_FOR_GC_SET_POSITION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_WAITING_FOR_GC_SET_POSITION);
    pPacketIDSet->addPacketID(Packet::PACKET_GC_SET_POSITION);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_WAITING_FOR_GC_RECONNECT_LOGIN
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_WAITING_FOR_GC_RECONNECT_LOGIN);
    pPacketIDSet->addPacketID(Packet::PACKET_GC_RECONNECT_LOGIN);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);


    //----------------------------------------------------------------------
    // CPS_NORMAL (into the game!)
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_NORMAL, PacketIDSet::PIST_ANY);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // CPS_END_SESSION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(CPS_END_SESSION, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

#elif defined(__LOGIN_SERVER__)

    //----------------------------------------------------------------------
    // LPS_NONE
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_NONE, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_BEGIN_SESSION ( right after the login session begins )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_BEGIN_SESSION);
    // In-client account registration: a fresh connection may query an ID or
    // register an account straight away, before any CLLogin.
    pPacketIDSet->addPacketID(Packet::PACKET_CL_QUERY_PLAYER_ID);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_REGISTER_PLAYER);
    // add by viva 2008-12-31
    pPacketIDSet->addPacketID(Packet::PACKET_CG_ENCODE_KEY);
    // end
    pPacketIDSet->addPacketID(Packet::PACKET_CL_LOGIN);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_RECONNECT_LOGIN);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_VERSION_CHECK);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_WAITING_FOR_CL_GET_PC_LIST ( before the user information is sent )
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_WAITING_FOR_CL_GET_PC_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_PC_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_WORLD_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_SERVER_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_WORLD);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_SERVER);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_QUERY_CHARACTER_NAME);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_CREATE_PC);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_WAITING_FOR_CL_REGISTER_PLAYER
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_WAITING_FOR_CL_REGISTER_PLAYER);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_REGISTER_PLAYER);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_QUERY_PLAYER_ID);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_PC_MANAGEMENT
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_PC_MANAGEMENT);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_PC_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_WORLD);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_SERVER);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_SELECT_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_WORLD_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_GET_SERVER_LIST);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_QUERY_CHARACTER_NAME);

    pPacketIDSet->addPacketID(Packet::PACKET_CL_CREATE_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_DELETE_PC);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_LOGOUT);
    pPacketIDSet->addPacketID(Packet::PACKET_CL_CHANGE_SERVER);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_AFTER_SENDING_LG_INCOMING_CONNECTION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_AFTER_SENDING_LG_INCOMING_CONNECTION, PacketIDSet::PIST_IGNORE_EXCEPT);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);


    //----------------------------------------------------------------------
    // LPS_AFTER_SENDING_LG_INCOMING_CONNECTION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_WAITING_FOR_GL_KICK_VERIFY, PacketIDSet::PIST_IGNORE_EXCEPT);
    pPacketIDSet->addPacketID(Packet::PACKET_GL_KICK_VERIFY);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // LPS_END_SESSION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(LPS_END_SESSION, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

#elif defined(__GAME_SERVER__)

    //----------------------------------------------------------------------
    // GPS_NONE
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_NONE, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_BEGIN_SESSION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_BEGIN_SESSION);
    // add by viva for Encode
    pPacketIDSet->addPacketID(Packet::PACKET_CG_ENCODE_KEY);
    //
    pPacketIDSet->addPacketID(Packet::PACKET_CG_CONNECT);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_WAITING_FOR_CG_READY
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_WAITING_FOR_CG_READY, PacketIDSet::PIST_IGNORE_EXCEPT);
    pPacketIDSet->addPacketID(Packet::PACKET_CG_READY);
    pPacketIDSet->addPacketID(Packet::PACKET_CG_SET_SLAYER_HOT_KEY);
    pPacketIDSet->addPacketID(Packet::PACKET_CG_SET_VAMPIRE_HOT_KEY);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_NORMAL ( loading is done, the position is settled, into the game )
    // Every registered packet a game client sends on this connection
    // (GameClientLink.h), and nothing else: the factory table also holds
    // the packets the gameserver sends and the ones its server links
    // carry, and a client must not make it read those.
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_NORMAL);
    for (PacketID_t packetID : de::packet::gameClientLinkPacketIDs())
        pPacketIDSet->addPacketID(packetID);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_IGNORE_ALL ( the state that takes no packet at all and only sends.. fucksuck)
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_IGNORE_ALL, PacketIDSet::PIST_IGNORE_EXCEPT);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_AFTER_SENDING_GL_INCOMING_CONNECTION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_AFTER_SENDING_GL_INCOMING_CONNECTION, PacketIDSet::PIST_IGNORE_EXCEPT);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

    //----------------------------------------------------------------------
    // GPS_END_SESSION
    //----------------------------------------------------------------------
    pPacketIDSet = new PacketIDSet(GPS_END_SESSION, PacketIDSet::PIST_NONE);
    addPacketIDSet(pPacketIDSet->getPlayerStatus(), pPacketIDSet);

#endif

    __END_DEBUG

    __END_CATCH
}

//----------------------------------------------------------------------
// add packet id set
//----------------------------------------------------------------------
void PacketValidator::addPacketIDSet(PlayerStatus playerStatus, PacketIDSet* pPacketIDSet) {
    __BEGIN_TRY

    if (playerStatus >= PLAYER_STATUS_MAX)
        throw Error("invalid player status");

    if (m_PacketIDSets[playerStatus] != NULL)
        throw DuplicatedException();

    m_PacketIDSets[playerStatus] = pPacketIDSet;

    __END_CATCH
}

//----------------------------------------------------------------------
// delete packet id set
//----------------------------------------------------------------------
void PacketValidator::deletePacketIDSet(PlayerStatus playerStatus, PacketIDSet* pPacketIDSet) {
    __BEGIN_TRY

    if (playerStatus >= PLAYER_STATUS_MAX)
        throw Error("invalid player status");

    if (m_PacketIDSets[playerStatus] == NULL)
        throw NoSuchElementException("invalid player status");

    m_PacketIDSets[playerStatus] = NULL;

    __END_CATCH
}

//----------------------------------------------------------------------
// is valid packet?
//----------------------------------------------------------------------
bool PacketValidator::isValidPacketID(PlayerStatus playerStatus, PacketID_t packetID) {
    __BEGIN_TRY

    if (playerStatus >= PLAYER_STATUS_MAX)
        throw Error("invalid player status");

    if (m_PacketIDSets[playerStatus] == NULL)
        throw NoSuchElementException("invalid player status");

    return m_PacketIDSets[playerStatus]->hasPacketID(packetID);

    __END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string PacketValidator::toString() const {
    StringStream msg;

    msg << "PacketValidator(\n";

    for (uint i = 0; i < PLAYER_STATUS_MAX; i++) {
        msg << i << " : ";
        msg << ((m_PacketIDSets[i] != NULL) ? m_PacketIDSets[i]->toString() : "NULL");
    }

    msg << ")";

    return msg.toString();
}
