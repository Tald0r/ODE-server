//////////////////////////////////////////////////////////////////////////////
// Filename    : ConnectionInfo.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ConnectionInfo.h"

#include "StringStream.h"

ConnectionInfo::ConnectionInfo()

{
    m_Key = 0;
    m_ExpireTime.tv_sec = 0;
    m_ExpireTime.tv_usec = 0;
}

ConnectionInfo::~ConnectionInfo()

{}

string ConnectionInfo::toString() const

{
    StringStream msg;
    msg << "ConnectionInfo(ClientIP:" << m_ClientIP << ",PlayerID:" << m_PlayerID << ",PCName:" << m_PCName
        << ",ExpireTime:" << m_ExpireTime.tv_sec << "." << m_ExpireTime.tv_usec << ",KEY: " << m_Key << ")";
    return msg.toString();
}
