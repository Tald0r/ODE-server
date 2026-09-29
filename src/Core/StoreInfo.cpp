#include "StoreInfo.h"

#include "WireString.h"

void StoreItemInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    iStream.read(m_ItemExist);

    if (m_ItemExist != 0) {
        PCItemInfo::read(iStream);
        iStream.read(m_Price);
    }

    __END_CATCH
}

void StoreItemInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    oStream.write(m_ItemExist);

    if (m_ItemExist != 0) {
        PCItemInfo::write(oStream);
        oStream.write(m_Price);
    }

    __END_CATCH
}

PacketSize_t StoreInfo::getSize(bool toOther) const {
    PacketSize_t ret = szBYTE;
    if (toOther && m_Open == 0)
        return ret;

    ret += de::wire::stringWireSize(m_Sign) + szBYTE;

    vector<StoreItemInfo>::const_iterator itr = m_Items.begin();

    for (; itr != m_Items.end(); ++itr) {
        ret += itr->getSize();
    }

    return ret;
}

void StoreInfo::read(SocketInputStream& iStream, bool toOther) {
    __BEGIN_TRY

    iStream.read(m_Open);
    if (toOther && m_Open == 0)
        return;

    de::wire::readString(iStream, m_Sign, {0, MAX_SIGN_SIZE}, "Sign");

    BYTE ItemNum;
    iStream.read(ItemNum);

    // The record has MAX_ITEM_NUM window slots and write() sends them all,
    // so a count past them is malformed and must not index past the end.
    if (ItemNum > m_Items.size())
        throw InvalidProtocolException("store item count out of range");

    for (int i = 0; i < ItemNum; ++i) {
        m_Items[i].read(iStream);
    }

    __END_CATCH
}

void StoreInfo::write(SocketOutputStream& oStream, bool toOther) const {
    __BEGIN_TRY

    oStream.write(m_Open);
    if (toOther && m_Open == 0)
        return;

    de::wire::writeString(oStream, m_Sign, {0, MAX_SIGN_SIZE}, "Sign");

    BYTE ItemNum = m_Items.size();
    oStream.write(ItemNum);

    vector<StoreItemInfo>::const_iterator itr = m_Items.begin();

    for (; itr != m_Items.end(); ++itr) {
        itr->write(oStream);
    }

    __END_CATCH
}

void StoreOutlook::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    iStream.read(m_Open);
    if (m_Open == 0)
        return;

    de::wire::readString(iStream, m_Sign, {0, MAX_SIGN_SIZE}, "Sign");

    __END_CATCH
}

void StoreOutlook::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    oStream.write(m_Open);
    if (m_Open == 0)
        return;

    de::wire::writeString(oStream, m_Sign, {0, MAX_SIGN_SIZE}, "Sign");

    __END_CATCH
}
