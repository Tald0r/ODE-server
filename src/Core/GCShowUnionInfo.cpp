//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShowUnionInfo.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCShowUnionInfo.h"
#include <memory>

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCShowUnionInfo::~GCShowUnionInfo()

{
    __BEGIN_TRY
    while (!m_GuildList.empty()) {
        SingleGuildInfo* pGuildInfo = m_GuildList.front();

        SAFE_DELETE(pGuildInfo);

        m_GuildList.pop_front();
    }

    m_GuildList.clear();

    __END_CATCH_NO_RETHROW
}

PacketSize_t GCShowUnionInfo::getPacketSize() const {
    PacketSize_t ret = m_MasterGuildInfo.getSize() + szBYTE;

    list<SingleGuildInfo*>::const_iterator itr = m_GuildList.begin();

    for (; itr != m_GuildList.end(); ++itr) {
        ret += (*itr)->getSize();
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCShowUnionInfo::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    m_MasterGuildInfo.read(iStream);

    BYTE szNum;
    iStream.read(szNum);
    for (int i = 0; i < szNum; ++i) {
        std::unique_ptr<SingleGuildInfo> pInfo(new SingleGuildInfo);
        pInfo->read(iStream);
        m_GuildList.push_back(pInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCShowUnionInfo::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    m_MasterGuildInfo.write(oStream);

    BYTE szNum = m_GuildList.size();
    oStream.write(szNum);

    list<SingleGuildInfo*>::const_iterator itr = m_GuildList.begin();

    for (; itr != m_GuildList.end(); ++itr) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCShowUnionInfo::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCShowUnionInfo(" << ")";

    return msg.toString();

    __END_CATCH
}
