//////////////////////////////////////////////////////////////////////////////
// Filename    : GCShopList.cpp
// Description :
// When the shop version the player holds differs from the server's,
// the player asks the server for the list of goods in the shop.
// This packet is the one handed to the player at that point.
//////////////////////////////////////////////////////////////////////////////

#include "GCShopList.h"

#include "Assert1.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
GCShopList::GCShopList()

{
    __BEGIN_TRY

    __END_CATCH;
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////
GCShopList::~GCShopList()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW;
}

//////////////////////////////////////////////////////////////////////////////
// Read data from the input stream (buffer) and initialise the packet.
//////////////////////////////////////////////////////////////////////////////
void GCShopList::read(SocketInputStream& iStream)

{
    __BEGIN_TRY


    BYTE i = 0;
    BYTE nTotal = 0;
    BYTE index = 0;

    // The listing replaces the rack the packet holds.
    for (i = 0; i < SHOP_RACK_INDEX_MAX; i++)
        m_pBuffer[i] = _SHOPLISTITEM();

    // read NPC id & version & rack type & number of item
    iStream.read(m_ObjectID);
    iStream.read(m_Version);
    iStream.read(m_RackType);
    iStream.read(nTotal);

    // read each item info
    for (i = 0; i < nTotal; i++) {
        iStream.read(index);
        if (index >= SHOP_RACK_INDEX_MAX)
            throw InvalidProtocolException("shop slot out of range");
        _SHOPLISTITEM& item = m_pBuffer[index];
        iStream.read(item.objectID);
        iStream.read(item.itemClass);
        iStream.read(item.itemType);

        BYTE optionSize;
        iStream.read(optionSize);

        if (optionSize > kMaxOptionCount)
            throw InvalidProtocolException("too many item options");

        for (int j = 0; j < optionSize; j++) {
            OptionType_t optionType;
            iStream.read(optionType);
            item.optionType.push_back(optionType);
        }

        iStream.read(item.durability);
        iStream.read(item.silver);
        iStream.read(item.grade);
        iStream.read(item.enchantLevel);

        m_pBuffer[index].bExist = true;
    }

    iStream.read(m_MarketCondBuy);
    iStream.read(m_MarketCondSell);
    iStream.read(m_ShopType);

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void GCShopList::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    BYTE i = 0;
    BYTE nTotal = 0;

    // calculate total number of items
    for (i = 0; i < SHOP_RACK_INDEX_MAX; i++)
        if (m_pBuffer[i].bExist)
            nTotal++;

    // write NPC id & version & rack type & number of item
    oStream.write(m_ObjectID);
    oStream.write(m_Version);
    oStream.write(m_RackType);
    oStream.write(nTotal);

    // write each item info
    for (i = 0; i < SHOP_RACK_INDEX_MAX; i++) {
        const _SHOPLISTITEM& item = m_pBuffer[i];

        if (item.bExist) {
            oStream.write(i);
            oStream.write(item.objectID);
            oStream.write(item.itemClass);
            oStream.write(item.itemType);

            if (item.optionType.size() > kMaxOptionCount)
                throw InvalidProtocolException("too many item options");

            BYTE optionSize = item.optionType.size();
            oStream.write(optionSize);

            list<OptionType_t>::const_iterator itr;
            for (itr = item.optionType.begin(); itr != item.optionType.end(); itr++) {
                OptionType_t optionType = *itr;
                oStream.write(optionType);
            }

            oStream.write(item.durability);
            oStream.write(item.silver);
            oStream.write(item.grade);
            oStream.write(item.enchantLevel);
        }
    }

    oStream.write(m_MarketCondBuy);
    oStream.write(m_MarketCondSell);
    oStream.write(m_ShopType);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

PacketSize_t GCShopList::getPacketSize() const

{
    PacketSize_t size = szObjectID +     // NPC id
                        szShopVersion +  // shop version
                        szShopRackType + // rack type
                        szBYTE;          // total number of item

    for (int i = 0; i < SHOP_RACK_INDEX_MAX; i++) {
        if (m_pBuffer[i].bExist) {
            size += szBYTE;                                                 // Item index in shop rack
            size += szObjectID;                                             // item OID
            size += szBYTE;                                                 // item class
            size += szItemType;                                             // item type
            size += szBYTE + szOptionType * m_pBuffer[i].optionType.size(); // item option type
            size += szDurability;                                           // item durability
            size += szSilver;                                               // silver coating amount
            size += szGrade;
            size += szEnchantLevel; // enchant level
        }
    }

    size += szMarketCond + szMarketCond;
    size += szBYTE; // shop type, written after the market conditions

    return size;
}


//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string GCShopList::toString() const

{
    __BEGIN_TRY

    StringStream msg;

    msg << "GCShopList(" << "ObjectID:" << (int)m_ObjectID << "," << "ShopVersion:" << (int)m_Version << ","
        << "RackType:" << (int)m_RackType << ",";

    for (int i = 0; i < SHOP_RACK_INDEX_MAX; i++) {
        msg << "(Item" << i << ":";
        if (m_pBuffer[i].bExist) {
            msg << "ObjectID:" << (int)(m_pBuffer[i].objectID) << "ItemClass:" << (int)(m_pBuffer[i].itemClass)
                << "ItemType:" << (int)(m_pBuffer[i].itemType)
                << "OptionTypeSize:" << (int)(m_pBuffer[i].optionType.size())
                << "Durability:" << (int)(m_pBuffer[i].durability) << "Silver:" << (int)(m_pBuffer[i].silver)
                << "Grade:" << (int)(m_pBuffer[i].grade) << "EnchantLevel:" << (int)(m_pBuffer[i].enchantLevel);
            msg << ")";
        }
    }

    msg << "MarketCondBuy:" << (int)m_MarketCondBuy << "MarketCondSell:" << (int)m_MarketCondSell
        << "ShopType:" << (int)m_ShopType;

    msg << ")";

    return msg.toString();

    __END_CATCH
}

void GCShopList::setShopType(const ShopRackType_t type) {
    m_RackType = type;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
SHOPLISTITEM GCShopList::getShopItem(BYTE index) const

{
    // check bound
    if (index >= SHOP_RACK_INDEX_MAX)
        throw InvalidProtocolException("GCShopList::getShopItem() : Out of Bound!");

    // return shop item info
    return m_pBuffer[index];
}

// setShopItem(BYTE, const Item*) is defined in the gameserver
// (packetfill/GCShopListFill.cpp): it reads a live Item, which the wire
// library must not depend on.
