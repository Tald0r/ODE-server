//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGuildMemberList.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCGuildMemberList.h"

#include <list>
#include <memory>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCGuildMemberList::GCGuildMemberList()

{
    m_Type = 0;
}

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCGuildMemberList::~GCGuildMemberList()

{
    __BEGIN_TRY

    // Delete every object in the guild list
    clearGuildMemberInfoList();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCGuildMemberList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    iStream.read(m_Type);

    BYTE ListNum;

    iStream.read(ListNum);

    if (ListNum > GuildMemberInfo::kMaxCount)
        throw InvalidProtocolException("too many guild member infos");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<GuildMemberInfo> pGuildMemberInfo(new GuildMemberInfo);
        pGuildMemberInfo->read(iStream);
        m_GuildMemberInfoList.push_back(pGuildMemberInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCGuildMemberList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_GuildMemberInfoList.size() > GuildMemberInfo::kMaxCount)
        throw InvalidProtocolException("too many guild member infos");

    oStream.write(m_Type);
    BYTE ListNum = m_GuildMemberInfoList.size();
    oStream.write(ListNum);

    GuildMemberInfoListConstItor itr = m_GuildMemberInfoList.begin();
    for (; itr != m_GuildMemberInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


void GCGuildMemberList::clearGuildMemberInfoList()

{
    __BEGIN_TRY

    // Delete the GuildMemberInfoList
    while (!m_GuildMemberInfoList.empty()) {
        GuildMemberInfo* pGuildMemberInfo = m_GuildMemberInfoList.front();
        m_GuildMemberInfoList.pop_front();
        SAFE_DELETE(pGuildMemberInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet size
//////////////////////////////////////////////////////////////////////
PacketSize_t GCGuildMemberList::getPacketSize() const

{
    __BEGIN_TRY

    PacketSize_t PacketSize = szBYTE + szBYTE;

    GuildMemberInfoListConstItor itr = m_GuildMemberInfoList.begin();

    for (; itr != m_GuildMemberInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCGuildMemberList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCGuildMemberList(";

    list<GuildMemberInfo*>::const_iterator itr = m_GuildMemberInfoList.begin();
    for (; itr != m_GuildMemberInfoList.end(); itr++) {
        msg << (*itr)->toString();
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
