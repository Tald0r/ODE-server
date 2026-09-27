//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddInventoryToMouse.h
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __CG_ADD_INVENTORY_TO_MOUSE_H__
#define __CG_ADD_INVENTORY_TO_MOUSE_H__

#include "Packet.h"
#include "PacketFactory.h"

//////////////////////////////////////////////////////////////////////////////
// class CGAddInventoryToMouse;
//////////////////////////////////////////////////////////////////////////////

class CGAddInventoryToMouse : public Packet {
public:
    CGAddInventoryToMouse();
    ~CGAddInventoryToMouse();

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_CG_ADD_INVENTORY_TO_MOUSE;
    }
    PacketSize_t getPacketSize() const {
        return szObjectID + szCoordInven + szCoordInven;
    }
    string getPacketName() const {
        return "CGAddInventoryToMouse";
    }
    string toString() const;

public:
    ObjectID_t getObjectID() {
        return m_ObjectID;
    }
    void setObjectID(ObjectID_t ObjectID) {
        m_ObjectID = ObjectID;
    }

    // ObjectID_t getInventoryItemObjectID()  { return m_InventoryItemObjectID; }
    // void setInventoryItemObjectID(ObjectID_t InventoryItemObjectID)  { m_InventoryItemObjectID =
    // InventoryItemObjectID; }

    CoordInven_t getX() const {
        return m_InvenX;
    }
    void setX(CoordInven_t InvenX) {
        m_InvenX = InvenX;
    }

    CoordInven_t getY() const {
        return m_InvenY;
    }
    void setY(CoordInven_t InvenY) {
        m_InvenY = InvenY;
    }

private:
    ObjectID_t m_ObjectID = 0;

    // Object id of the sub inventory item. 0 means it is taken from the main inventory
    [[maybe_unused]] ObjectID_t m_InventoryItemObjectID = 0;

    CoordInven_t m_InvenX = 0;
    CoordInven_t m_InvenY = 0;
};

//////////////////////////////////////////////////////////////////////////////
// class CGAddInventoryToMouseFactory;
//////////////////////////////////////////////////////////////////////////////

class CGAddInventoryToMouseFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_CG_ADD_INVENTORY_TO_MOUSE;
    static constexpr std::string_view kName = "CGAddInventoryToMouse";
    static constexpr PacketSize_t kMaxSize{szObjectID + szCoordInven + szCoordInven};

    Packet* createPacket() override {
        return new CGAddInventoryToMouse();
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

//////////////////////////////////////////////////////////////////////////////
// class CGAddInventoryToMouseHandler;
//////////////////////////////////////////////////////////////////////////////

class CGAddInventoryToMouseHandler {
public:
    static void execute(CGAddInventoryToMouse* pPacket, Player* player);
};

#endif
