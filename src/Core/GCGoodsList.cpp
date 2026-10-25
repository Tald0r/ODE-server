//////////////////////////////////////////////////////////////////////////////
// Filename    : GCGoodsList.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "GCGoodsList.h"

#include <memory>

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

GCGoodsList::GCGoodsList()

{
    __BEGIN_TRY

    m_GoodsList.clear();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCGoodsList::~GCGoodsList()

{
    __BEGIN_TRY

    clearGoodsInfo();

    __END_CATCH_NO_RETHROW
}

//////////////////////////////////////////////////////////////////////////////
// Free every record the packet holds.
//////////////////////////////////////////////////////////////////////////////
void GCGoodsList::clearGoodsInfo()

{
    list<GoodsInfo*>::iterator itr = m_GoodsList.begin();
    list<GoodsInfo*>::iterator endItr = m_GoodsList.end();

    for (; itr != endItr; ++itr) {
        if (*itr != NULL)
            SAFE_DELETE(*itr);
    }

    m_GoodsList.clear();
}

//////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////////////
void GCGoodsList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    BYTE totalNum;
    iStream.read(totalNum);
    if (totalNum > MAX_GOODS_LIST)
        throw DisconnectException("GCGoodsList : totalNum greater than MAX_GOODS_LIST");

    clearGoodsInfo();

    for (int i = 0; i < totalNum; ++i) {
        std::unique_ptr<GoodsInfo> pGI(new GoodsInfo);

        iStream.read(pGI->objectID);
        iStream.read(pGI->itemClass);
        iStream.read(pGI->itemType);
        iStream.read(pGI->grade);

        BYTE optionNum;
        iStream.read(optionNum);

        pGI->optionType.clear();

        for (int j = 0; j < optionNum; ++j) {
            OptionType_t optionType;
            iStream.read(optionType);
            pGI->optionType.push_back(optionType);
        }

        iStream.read(pGI->num);
        iStream.read(pGI->timeLimit);

        addGoodsInfo(pGI.release());
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void GCGoodsList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    if (m_GoodsList.size() > (size_t)MAX_GOODS_LIST)
        throw DisconnectException("GCGoodsList : totalNum greater than MAX_GOODS_LIST");

    BYTE totalNum = (BYTE)m_GoodsList.size();
    oStream.write(totalNum);

    list<GoodsInfo*>::const_iterator itr = m_GoodsList.begin();
    list<GoodsInfo*>::const_iterator endItr = m_GoodsList.end();

    for (; itr != endItr; ++itr) {
        GoodsInfo* pGI = *itr;
        if (pGI == NULL)
            throw InvalidProtocolException("GCGoodsList : null goods record");

        oStream.write(pGI->objectID);
        oStream.write(pGI->itemClass);
        oStream.write(pGI->itemType);
        oStream.write(pGI->grade);

        if (pGI->optionType.size() > kMaxOptionCount)
            throw InvalidProtocolException("GCGoodsList : too many record options");

        BYTE optionNum = (BYTE)pGI->optionType.size();
        oStream.write(optionNum);

        list<OptionType_t>::const_iterator oitr = pGI->optionType.begin();
        list<OptionType_t>::const_iterator endoItr = pGI->optionType.end();

        for (; oitr != endoItr; ++oitr) {
            oStream.write(*oitr);
        }

        oStream.write(pGI->num);
        oStream.write(pGI->timeLimit);
    }

    __END_DEBUG
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
PacketSize_t GCGoodsList::getPacketSize() const

{
    __BEGIN_TRY
    __BEGIN_DEBUG

    PacketSize_t size = szBYTE;

    list<GoodsInfo*>::const_iterator itr = m_GoodsList.begin();
    list<GoodsInfo*>::const_iterator endItr = m_GoodsList.end();

    for (; itr != endItr; ++itr) {
        size += (*itr)->getPacketSize();
    }

    return size;

    __END_DEBUG
    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string GCGoodsList::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "GCGoodsList(";

    list<GoodsInfo*>::const_iterator itr = m_GoodsList.begin();
    list<GoodsInfo*>::const_iterator endItr = m_GoodsList.end();

    for (; itr != endItr; ++itr) {
        msg << (*itr)->toString();
    }

    msg << ")";
    return msg.toString();

    __END_CATCH
}
