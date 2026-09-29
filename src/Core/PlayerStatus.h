//----------------------------------------------------------------------
//
// Filename    : PlayerStatus.h
// Written By  : Reiot
// Description : Mapping of the packet ids allowed per player status
//
//----------------------------------------------------------------------

#ifndef __PLAYER_STATUS_H__
#define __PLAYER_STATUS_H__

enum PlayerStatus {

#if defined(__GAME_CLIENT__)

    //----------------------------------------------------------------------
    // Right after the ClientPlayer object is created
    // Next Packets : NONE
    //----------------------------------------------------------------------
    CPS_NONE,

    //----------------------------------------------------------------------
    // Before the login information is sent to the login server
    // Next Packets : LCLoginOK, LCLoginError
    //----------------------------------------------------------------------
    CPS_BEGIN_SESSION,

    //----------------------------------------------------------------------
    // Right after sending CLLogin
    // Next Packets : LCLoginOK, LCLoginError
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_LOGIN,

    //----------------------------------------------------------------------
    // Right after sending CLQueryPlayerID
    // Next Packets : LCQueryResultPlayerID
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_QUERY_PLAYER_ID,

    //----------------------------------------------------------------------
    // Right after sending CLRegisterPlayer
    // Next Packets : LCRegisterPlayerOK, LCRegisterPlayerError
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_REGISTER_PLAYER,

    //----------------------------------------------------------------------
    // Right after sending the CLGetPCList packet
    // Next Packets : LCPCList
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_GET_PC_LIST,

    //----------------------------------------------------------------------
    // Right after sending CLCreatePC
    // Next Packets : LCCreatePCOK
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_CREATE_PC,

    //----------------------------------------------------------------------
    // Right after sending CLDeletePC
    // Next Packets : LCDeletePCOK, LCDeletePCError
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_DELETE_PC,

    //----------------------------------------------------------------------
    // Right after sending CLSelectPC
    // Next Packets : LCSelectPCOK, LCSelectPCError
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CL_SELECT_PC,

    //----------------------------------------------------------------------
    // Send the CGConnect packet to the game server.
    // Next Packets : GCUpdateInfo
    //----------------------------------------------------------------------
    CPS_AFTER_SENDING_CG_CONNECT,

    //----------------------------------------------------------------------
    // Wait until loading finishes.
    // Next Packets : NONE
    //----------------------------------------------------------------------
    CPS_WAITING_FOR_LOADING,

    //----------------------------------------------------------------------
    // After sending the CGReady packet, wait for the server to settle its own position.
    // Next Packets : GCSetPosition
    //----------------------------------------------------------------------
    CPS_WAITING_FOR_GC_SET_POSITION,

    //----------------------------------------------------------------------
    // Normal game state
    //----------------------------------------------------------------------
    CPS_NORMAL,

    //----------------------------------------------------------------------
    // CGReconnectLogin after LOGOUT
    //----------------------------------------------------------------------
    CPS_WAITING_FOR_GC_RECONNECT_LOGIN,

    //----------------------------------------------------------------------
    // Connection to the login/game server closed
    //----------------------------------------------------------------------
    CPS_END_SESSION,

/*
//----------------------------------------------------------------------
// Game bulletin board start
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_SELECT_BOARD
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_SELECT_BOARD,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_WRITE_TEXT
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_WRITE_CONTENT,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_SELECT_TEXT
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_SELECT_TEXT,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_RELPLY_CONTENT
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_REPLY_CONTENT,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_SEND_PASSWORD
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_SEND_PASSWORD,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_MODIFY_CONTENT
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_MODIFY_CONTENT,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_PREV_PAGE
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_PREV_PAGE,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_NEXT_PAGE
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_NEXT_PAGE,

//----------------------------------------------------------------------
// CPS_AFTER_SENDING_CL_SELECT_LIST
//----------------------------------------------------------------------
CPS_AFTER_SENDING_CL_SELECT_LIST,
//----------------------------------------------------------------------
// Game bulletin board end
//----------------------------------------------------------------------
*/

#elif defined(__LOGIN_SERVER__)

    //----------------------------------------------------------------------
    // Right after the LoginPlayer object is created
    // Next Packets : NONE
    //----------------------------------------------------------------------
    LPS_NONE,

    //----------------------------------------------------------------------
    // Right after the socket connection to the login server
    // Next Packets : CLLogin
    //----------------------------------------------------------------------
    LPS_BEGIN_SESSION,

    //----------------------------------------------------------------------
    // Right after the id/password is sent
    // Next Packets : CLGetPCList, CLLogout
    //----------------------------------------------------------------------
    LPS_WAITING_FOR_CL_GET_PC_LIST,

    //----------------------------------------------------------------------
    // Logged on as a guest
    // Next Packets : CLRegisterPlayer, CLQueryPlayerID
    //----------------------------------------------------------------------
    LPS_WAITING_FOR_CL_REGISTER_PLAYER,

    //----------------------------------------------------------------------
    // Character management state
    // Next Packets : CLCreatePC, CLDeletePC, CLSelectPC, CLLogout
    //----------------------------------------------------------------------
    LPS_PC_MANAGEMENT,

    //----------------------------------------------------------------------
    // Right after sending the LGIncomingConnection packet to the game server
    // Next Packets : GLIncomingConnectionOK, GLIncomingConnectionError
    //----------------------------------------------------------------------
    LPS_AFTER_SENDING_LG_INCOMING_CONNECTION,

    //----------------------------------------------------------------------
    // The 'already connected' case..
    // State waiting for the connection to be closed by force
    //----------------------------------------------------------------------
    LPS_WAITING_FOR_GL_KICK_VERIFY,


    //----------------------------------------------------------------------
    // When the connection to the login server is closed
    // Next Packets : NONE
    //----------------------------------------------------------------------
    LPS_END_SESSION,


/*
//----------------------------------------------------------------------
// Game bulletin board start
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// LPS_WAITHING_FOR_CL_SELECT_BOARD
//----------------------------------------------------------------------
 LPS_WAITING_FOR_CL_SELECT_BOARD,

//----------------------------------------------------------------------
// LPS_AFTER_SENDING_CL_TEXT_LIST
//----------------------------------------------------------------------
 LPS_AFTER_SENDING_LC_TEXT_LIST,

//----------------------------------------------------------------------
// LPS_AFTER_SENDING_CL_SENDING_CONTENT
//----------------------------------------------------------------------
 LPS_AFTER_SENDING_LC_SEND_CONTENT,

//----------------------------------------------------------------------
// Game bulletin board end
//----------------------------------------------------------------------
*/

#elif defined(__GAME_SERVER__)

    //----------------------------------------------------------------------
    // Right after the GamePlayer object is created
    // Next Packets : NONE
    //----------------------------------------------------------------------
    GPS_NONE,

    //----------------------------------------------------------------------
    // Right after the socket connection to the game server
    // Next Packets : CGConnect
    //----------------------------------------------------------------------
    GPS_BEGIN_SESSION,

    //----------------------------------------------------------------------
    // Right after the user is authenticated
    // Next Packets : CGReady
    //----------------------------------------------------------------------
    GPS_WAITING_FOR_CG_READY,

    //----------------------------------------------------------------------
    // In the game now. Any packet a client sends may arrive.
    // Next Packets : every client-sent packet (GameClientLink.h)
    //----------------------------------------------------------------------
    GPS_NORMAL,

    //----------------------------------------------------------------------
    // The state where no packet at all is accepted. -_-; fuck suck
    //----------------------------------------------------------------------
    GPS_IGNORE_ALL,

    //----------------------------------------------------------------------
    // Right after sending the GLIncomingConnection packet to the login server
    // Next Packets : LGIncomingConnectionOK, LGIncomingConnectionError
    //----------------------------------------------------------------------
    GPS_AFTER_SENDING_GL_INCOMING_CONNECTION,

    //----------------------------------------------------------------------
    // Close the connection to the game server.
    // Next Packets : NONE
    //----------------------------------------------------------------------
    GPS_END_SESSION,


#endif

    //--------------------------------------------------
    // Used when setting the size of the Player Status array and the like.
    //--------------------------------------------------
    PLAYER_STATUS_MAX

};

#endif
