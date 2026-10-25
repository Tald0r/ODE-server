//////////////////////////////////////////////////////////////////////
//
// Filename    : GCUnionOfferList.cpp
// Written By  :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "GCUnionOfferList.h"

#include <memory>

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
GCUnionOfferList::~GCUnionOfferList()

{
    __BEGIN_TRY
    while (!m_UnionOfferList.empty()) {
        SingleGuildUnionOffer* pUnionOffer = m_UnionOfferList.front();

        SAFE_DELETE(pUnionOffer);

        m_UnionOfferList.pop_front();
    }

    m_UnionOfferList.clear();

    __END_CATCH_NO_RETHROW
}

PacketSize_t GCUnionOfferList::getPacketSize() const {
    PacketSize_t ret = szBYTE;

    list<SingleGuildUnionOffer*>::const_iterator itr = m_UnionOfferList.begin();

    for (; itr != m_UnionOfferList.end(); ++itr) {
        ret += (*itr)->getSize();
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////
void GCUnionOfferList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    BYTE szNum;
    iStream.read(szNum);

    if (szNum > SingleGuildUnionOffer::kMaxCount)
        throw InvalidProtocolException("too many union offers");

    for (int i = 0; i < szNum; ++i) {
        std::unique_ptr<SingleGuildUnionOffer> pInfo(new SingleGuildUnionOffer);
        pInfo->read(iStream);
        m_UnionOfferList.push_back(pInfo.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////
void GCUnionOfferList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_UnionOfferList.size() > SingleGuildUnionOffer::kMaxCount)
        throw InvalidProtocolException("too many union offers");

    BYTE szNum = m_UnionOfferList.size();
    oStream.write(szNum);

    list<SingleGuildUnionOffer*>::const_iterator itr = m_UnionOfferList.begin();

    for (; itr != m_UnionOfferList.end(); ++itr) {
        (*itr)->write(oStream);
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
string GCUnionOfferList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCUnionOfferList(" << ")";

    return msg.toString();

    __END_CATCH
}
