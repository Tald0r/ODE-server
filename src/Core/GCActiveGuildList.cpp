//////////////////////////////////////////////////////////////////////
//
// Filename    : GCActiveGuildList.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCActiveGuildList.h"

#include <exception>
#include <list>
#include <memory>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCActiveGuildList::GCActiveGuildList()

{}

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
GCActiveGuildList::~GCActiveGuildList()

{
    // Destructor must not throw; clear list defensively.
    try {
        clearGuildInfoList();
    } catch (const std::exception&) {
        // Ignore to avoid std::terminate in noexcept destructor.
    }
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCActiveGuildList::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    WORD ListNum;

    iStream.read(ListNum);

    if (ListNum > GuildInfo::kMaxCount)
        throw InvalidProtocolException("too many guild infos");

    for (int i = 0; i < ListNum; i++) {
        std::unique_ptr<GuildInfo> pGuildInfo(new GuildInfo);
        pGuildInfo->read(iStream);
        m_GuildInfoList.push_back(pGuildInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCActiveGuildList::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    if (m_GuildInfoList.size() > GuildInfo::kMaxCount)
        throw InvalidProtocolException("too many guild infos");

    WORD ListNum = m_GuildInfoList.size();
    oStream.write(ListNum);

    GuildInfoListConstItor itr = m_GuildInfoList.begin();
    for (; itr != m_GuildInfoList.end(); itr++) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


void GCActiveGuildList::clearGuildInfoList()

{
    __BEGIN_TRY

    // Delete the GuildInfoList
    while (!m_GuildInfoList.empty()) {
        GuildInfo* pGuildInfo = m_GuildInfoList.front();
        m_GuildInfoList.pop_front();
        SAFE_DELETE(pGuildInfo);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet size
//////////////////////////////////////////////////////////////////////
PacketSize_t GCActiveGuildList::getPacketSize() const

{
    __BEGIN_TRY

    PacketSize_t PacketSize = szWORD;

    GuildInfoListConstItor itr = m_GuildInfoList.begin();

    for (; itr != m_GuildInfoList.end(); itr++) {
        PacketSize += (*itr)->getSize();
    }

    return PacketSize;

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCActiveGuildList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCActiveGuildList(";

    list<GuildInfo*>::const_iterator itr = m_GuildInfoList.begin();
    for (; itr != m_GuildInfoList.end(); itr++) {
        msg << (*itr)->toString();
    }

    msg << ")";

    return msg.toString();

    __END_CATCH
}
