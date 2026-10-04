//////////////////////////////////////////////////////////////////////
//
// Filename    : LGKickCharacter.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "LGKickCharacter.h"


//////////////////////////////////////////////////////////////////////
// Read data from the Datagram object and initialise the packet.
//////////////////////////////////////////////////////////////////////
void LGKickCharacter::read(Datagram& iDatagram)

{
    __BEGIN_TRY

    iDatagram.read(m_ID);

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

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the Datagram object.
//////////////////////////////////////////////////////////////////////
void LGKickCharacter::write(Datagram& oDatagram) const

{
    __BEGIN_TRY

    oDatagram.write(m_ID);

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

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
string LGKickCharacter::toString() const

{
    StringStream msg;

    msg << "LGKickCharacter(" << "ID:" << m_ID << ",PCName:" << m_PCName << ")";

    return msg.toString();
}
