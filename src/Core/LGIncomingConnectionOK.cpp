//////////////////////////////////////////////////////////////////////
//
// Filename    : LGIncomingConnectionOK.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LGIncomingConnectionOK.h"


//////////////////////////////////////////////////////////////////////
// Read data from the Datagram object and initialise the packet.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnectionOK::read(Datagram& iDatagram)

{
    __BEGIN_TRY

    //--------------------------------------------------
    // read Player ID
    //--------------------------------------------------
    BYTE szPlayerID;

    iDatagram.read(szPlayerID);

    if (szPlayerID == 0)
        throw InvalidProtocolException("szPlayerID == 0");

    if (szPlayerID > 20)
        throw InvalidProtocolException("too long playerID size");

    iDatagram.read(m_PlayerID, szPlayerID);

    //--------------------------------------------------
    // read TCP Port
    //--------------------------------------------------
    iDatagram.read(m_TCPPort);

    //--------------------------------------------------
    // read Auth Key
    //--------------------------------------------------
    iDatagram.read(m_Key);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the Datagram object.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnectionOK::write(Datagram& oDatagram) const

{
    __BEGIN_TRY

    //--------------------------------------------------
    // write Player ID
    //--------------------------------------------------
    const auto szPlayerID = m_PlayerID.size();

    if (szPlayerID == 0)
        throw InvalidProtocolException("szPlayerID == 0");

    if (szPlayerID > 20)
        throw InvalidProtocolException("too long playerID size");

    oDatagram.write(static_cast<BYTE>(szPlayerID));

    oDatagram.write(m_PlayerID);

    //--------------------------------------------------
    // write TCP Port
    //--------------------------------------------------
    oDatagram.write(m_TCPPort);

    //--------------------------------------------------
    // write auth Key
    //--------------------------------------------------
    oDatagram.write(m_Key);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
string LGIncomingConnectionOK::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "LGIncomingConnectionOK(" << "PlayerID:" << m_PlayerID << ",TCPPort:" << m_TCPPort << ",Key:" << m_Key
        << ")";
    return msg.toString();

    __END_CATCH
}
