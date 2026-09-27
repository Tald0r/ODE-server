//////////////////////////////////////////////////////////////////////////////
// Filename    : GCShopListMysterious.h
// Description :
// When the shop version the player holds differs from the server's,
// the player asks the server for the list of goods in the shop.
// This packet is the one handed to the player at that point.
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_SHOP_LIST_MYSTERIOUS_H__
#define __GC_SHOP_LIST_MYSTERIOUS_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// buffer structure
//////////////////////////////////////////////////////////////////////////////
typedef struct SHOPLISTITEM_MYSTERIOUS {
    bool bExist = false;
    BYTE itemClass = 0;
    ItemType_t itemType = 0;

} SHOPLISTITEM_MYSTERIOUS;


//////////////////////////////////////////////////////////////////////////////
// class GCShopListMysterious;
//////////////////////////////////////////////////////////////////////////////

class Item;

class GCShopListMysterious : public Packet {
public:
    GCShopListMysterious();
    virtual ~GCShopListMysterious();

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_SHOP_LIST_MYSTERIOUS;
    }
    PacketSize_t getPacketSize() const;
    string getPacketName() const {
        return "GCShopListMysterious";
    }
    string toString() const;

public:
    ObjectID_t getObjectID() const {
        return m_ObjectID;
    }
    void setObjectID(ObjectID_t creatureID) {
        m_ObjectID = creatureID;
    }

    ShopVersion_t getShopVersion(void) const {
        return m_Version;
    }
    void setShopVersion(ShopVersion_t ver) {
        m_Version = ver;
    }

    ShopRackType_t getShopType(void) const {
        return m_RackType;
    }
    void setShopType(const ShopRackType_t type) {
        m_RackType = type;
    }

    SHOPLISTITEM_MYSTERIOUS getShopItem(BYTE index) const;
    void setShopItem(BYTE index, const Item* pItem);

    MarketCond_t getMarketCondBuy(void) const {
        return m_MarketCondBuy;
    }
    MarketCond_t getMarketCondSell(void) const {
        return m_MarketCondSell;
    }
    void setMarketCondBuy(MarketCond_t cond) {
        m_MarketCondBuy = cond;
    }
    void setMarketCondSell(MarketCond_t cond) {
        m_MarketCondSell = cond;
    }

private:
    ObjectID_t m_ObjectID = 0;                              // NPC's object id
    ShopVersion_t m_Version = 0;                            // Shop version
    ShopRackType_t m_RackType = 0;                          // rack type
    SHOPLISTITEM_MYSTERIOUS m_pBuffer[SHOP_RACK_INDEX_MAX]; // buffer for i/o
    MarketCond_t m_MarketCondBuy = 0;                       // Market condition
    MarketCond_t m_MarketCondSell = 0;
};

//////////////////////////////////////////////////////////////////////////////
// class GCShopListMysteriousFactory;
//////////////////////////////////////////////////////////////////////////////

class GCShopListMysteriousFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_SHOP_LIST_MYSTERIOUS;
    static constexpr std::string_view kName = "GCShopListMysterious";
    static constexpr PacketSize_t kMaxSize{[] {
        PacketSize_t unit_size = szBYTE + szItemType;
        PacketSize_t rValue = 0;

        rValue += szObjectID;                      // NPC ID
        rValue += szShopVersion;                   // shop version
        rValue += szShopRackType;                  // shop rack type
        rValue += szBYTE;                          // total number of item
        rValue += unit_size * SHOP_RACK_INDEX_MAX; // actual item info
        rValue += szMarketCond * 2;                // market condition

        return rValue;
    }()};

    Packet* createPacket() override {
        return new GCShopListMysterious();
    }
    string getPacketName() const override {
        return string(kName);
    }
    PacketID_t getPacketID() const override {
        return kPacketID;
    }
    PacketSize_t getPacketMaxSize() const override {
        return kMaxSize;
    }
};


//////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////

#endif
