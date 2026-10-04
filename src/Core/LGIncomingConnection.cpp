//////////////////////////////////////////////////////////////////////
//
// Filename    : LGIncomingConnection.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LGIncomingConnection.h"


//////////////////////////////////////////////////////////////////////
// Read data from the Datagram object and initialise the packet.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnection::read(Datagram& iDatagram)

{
    __BEGIN_TRY

    //--------------------------------------------------
    // read player id
    //--------------------------------------------------
    BYTE szPlayerID;

    iDatagram.read(szPlayerID);

    if (szPlayerID == 0)
        throw InvalidProtocolException("szPlayerID == 0");

    if (szPlayerID > 20)
        throw InvalidProtocolException("too long name length");

    iDatagram.read(m_PlayerID, szPlayerID);

    //--------------------------------------------------
    // read creature's name
    //--------------------------------------------------
    BYTE szPCName;

    iDatagram.read(szPCName);

    if (szPCName == 0)
        throw InvalidProtocolException("szPCName == 0");

    if (szPCName > 20)
        throw InvalidProtocolException("too long name length");

    iDatagram.read(m_PCName, szPCName);

    //--------------------------------------------------
    // read client IP
    //--------------------------------------------------
    BYTE szClientIP;

    iDatagram.read(szClientIP);

    if (szClientIP == 0)
        throw InvalidProtocolException("szClientIP == 0");

    if (szClientIP > 15)
        throw InvalidProtocolException("too long IP length");

    iDatagram.read(m_ClientIP, szClientIP);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the Datagram object.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnection::write(Datagram& oDatagram) const

{
    __BEGIN_TRY

    //--------------------------------------------------
    // write player id
    //--------------------------------------------------
    const auto szPlayerID = m_PlayerID.size();

    if (szPlayerID == 0)
        throw InvalidProtocolException("szPlayerID == 0");

    if (szPlayerID > 20)
        throw InvalidProtocolException("too long name length");

    oDatagram.write(static_cast<BYTE>(szPlayerID));

    oDatagram.write(m_PlayerID);

    //--------------------------------------------------
    // write PC name
    //--------------------------------------------------
    const auto szPCName = m_PCName.size();

    if (szPCName == 0)
        throw InvalidProtocolException("szPCName == 0");

    if (szPCName > 20)
        throw InvalidProtocolException("too long name length");

    oDatagram.write(static_cast<BYTE>(szPCName));

    oDatagram.write(m_PCName);

    //--------------------------------------------------
    // write client IP
    //--------------------------------------------------
    const auto szClientIP = m_ClientIP.size();

    if (szClientIP == 0)
        throw InvalidProtocolException("szClientIP == 0");

    if (szClientIP > 15)
        throw InvalidProtocolException("too long IP length");

    oDatagram.write(static_cast<BYTE>(szClientIP));

    oDatagram.write(m_ClientIP);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
string LGIncomingConnection::toString() const

{
    StringStream msg;

    msg << "LGIncomingConnection(" << "PlayerID:" << m_PlayerID << ",PCName:" << m_PCName << ",ClientIP:" << m_ClientIP
        << ")";

    return msg.toString();
}
