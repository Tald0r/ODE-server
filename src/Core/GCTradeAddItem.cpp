////////////////////////////////////////////////////////////////////////////////
// Filename    : GCTradeAddItem.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////

// include files
#include "GCTradeAddItem.h"

#include <memory>

#include "Assert1.h"

////////////////////////////////////////////////////////////////////////////////
// constructor
////////////////////////////////////////////////////////////////////////////////
GCTradeAddItem::GCTradeAddItem()

{
    __BEGIN_TRY

    m_Grade = 0;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// destructor
////////////////////////////////////////////////////////////////////////////////
GCTradeAddItem::~GCTradeAddItem()

{
    __BEGIN_TRY

    while (!m_InfoList.empty()) {
        SubItemInfo* pInfo = m_InfoList.front();
        SAFE_DELETE(pInfo);
        m_InfoList.pop_front();
    }

    __END_CATCH_NO_RETHROW
}


////////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
////////////////////////////////////////////////////////////////////////////////
void GCTradeAddItem::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    iStream.read(m_TargetObjectID);
    iStream.read(m_X);
    iStream.read(m_Y);
    iStream.read(m_ItemObjectID);
    iStream.read(m_ItemClass);
    iStream.read(m_ItemType);

    BYTE optionSize;
    iStream.read(optionSize);

    if (optionSize > kMaxOptionTypes)
        throw InvalidProtocolException("too many option types");

    for (int i = 0; i < optionSize; i++) {
        OptionType_t optionType;
        iStream.read(optionType);
        addOptionType(optionType);
    }

    iStream.read(m_Durability);
    iStream.read(m_ItemNum);
    iStream.read(m_Silver);
    iStream.read(m_Grade);
    iStream.read(m_EnchantLevel);

    BYTE listNum;
    iStream.read(listNum);

    if (listNum > kMaxSubItems)
        throw InvalidProtocolException("too many sub items");

    for (BYTE i = 0; i < listNum; i++) {
        std::unique_ptr<SubItemInfo> pInfo(new SubItemInfo);
        pInfo->read(iStream);
        m_InfoList.push_back(pInfo.release());
    }

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
////////////////////////////////////////////////////////////////////////////////
void GCTradeAddItem::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    if (m_OptionType.size() > kMaxOptionTypes)
        throw InvalidProtocolException("too many option types");
    if (m_InfoList.size() > kMaxSubItems)
        throw InvalidProtocolException("too many sub items");

    oStream.write(m_TargetObjectID);
    oStream.write(m_X);
    oStream.write(m_Y);
    oStream.write(m_ItemObjectID);
    oStream.write(m_ItemClass);
    oStream.write(m_ItemType);

    BYTE optionSize = m_OptionType.size();
    oStream.write(optionSize);

    list<OptionType_t>::const_iterator iOption = m_OptionType.begin();
    for (; iOption != m_OptionType.end(); iOption++) {
        OptionType_t optionType = *iOption;
        oStream.write(optionType);
    }

    oStream.write(m_Durability);
    oStream.write(m_ItemNum);
    oStream.write(m_Silver);
    oStream.write(m_Grade);
    oStream.write(m_EnchantLevel);

    BYTE listNum = m_InfoList.size();
    oStream.write(listNum);

    list<SubItemInfo*>::const_iterator itr = m_InfoList.begin();
    for (; itr != m_InfoList.end(); itr++) {
        Assert(*itr != NULL);
        (*itr)->write(oStream);
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// get packet's debug string
////////////////////////////////////////////////////////////////////////////////
string GCTradeAddItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << " GCTradeAddItem(" << " TargetObjectID : " << (int)m_TargetObjectID
        << " ItemObjectID : " << (int)m_ItemObjectID << " ItemClass : " << (int)m_ItemClass
        << " ItemType : " << (int)m_ItemType << " OptionTypeSize : " << (int)m_OptionType.size()
        << " Durability : " << (int)m_Durability << " ItemNum : " << (int)m_ItemNum << " Silver : " << (int)m_Silver
        << " Grade : " << (int)m_Grade << " EnchantLevel : " << (int)m_EnchantLevel
        << " ListNum : " << (int)m_InfoList.size() << " )";
    return msg.toString();

    __END_CATCH
}
