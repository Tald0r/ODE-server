//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetInfo.h
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_PET_INFO_H__
#define __GC_PET_INFO_H__

#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"
#include "Types.h"

class GCPetInfo : public Packet {
public:
    GCPetInfo();
    virtual ~GCPetInfo();

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_PET_INFO;
    }
    PacketSize_t getPacketSize() const {
        return szObjectID + ((m_pPetInfo != NULL) ? (m_pPetInfo->getSize()) : szPetType);
    }
    string getPacketName() const {
        return "GCPetInfo";
    }
    string toString() const;

public:
    void setObjectID(ObjectID_t ObjectID) {
        m_ObjectID = ObjectID;
    }
    ObjectID_t getObjectID() const {
        return m_ObjectID;
    }

    BYTE isSummonInfo() const {
        return m_IsSummonInfo;
    }
    void setSummonInfo(BYTE isSummon) {
        m_IsSummonInfo = isSummon;
    }

    void setPetInfo(PetInfo* pPetInfo) {
        m_pPetInfo = pPetInfo;
    }
    PetInfo* getPetInfo() const {
        return m_pPetInfo;
    }

private:
    ObjectID_t m_ObjectID;
    PetInfo* m_pPetInfo;
    BYTE m_IsSummonInfo;
    bool m_bOwnsPetInfo = false; // true once read() allocated the record
};

class GCPetInfoFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_PET_INFO;
    static constexpr std::string_view kName = "GCPetInfo";
    static constexpr PacketSize_t kMaxSize{szObjectID + PetInfo::getMaxSize()};

    Packet* createPacket() override {
        return new GCPetInfo();
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

#endif // __GC_PET_INFO_H__
