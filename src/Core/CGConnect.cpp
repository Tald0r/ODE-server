//////////////////////////////////////////////////////////////////////////////
// Filename    : CGConnect.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGConnect.h"

#include "WireString.h"

void CGConnect::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    // read authentication key
    iStream.read(m_Key);

    // read PC type
    BYTE pcType;
    iStream.read(pcType);
    m_PCType = PCType(pcType);

    // read PC name
    de::wire::readString(iStream, m_PCName, {1, 20}, "PCName");
    iStream.read((char*)m_MacAddress, 6);

    __END_CATCH
}

void CGConnect::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    // write authentication key
    oStream.write(m_Key);

    // write PC type
    oStream.write((BYTE)m_PCType);

    // write PC name
    de::wire::writeString(oStream, m_PCName, {1, 20}, "PCName");

    oStream.write((char*)m_MacAddress, 6);

    __END_CATCH
}

string CGConnect::toString() const

{
    StringStream msg;
    msg << "CGConnect(" << "KEY:" << m_Key << ",PCType:" << nameOrNumber(PCType2String, m_PCType)
        << ",PCName:" << m_PCName << ")";
    return msg.toString();
}
