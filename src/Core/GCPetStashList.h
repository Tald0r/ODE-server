//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPetStashList.h
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_PET_STASH_LIST_H__
#define __GC_PET_STASH_LIST_H__

#include <vector>

#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"

#define MAX_PET_STASH 20

struct PetStashItemInfo {
    PetInfo* pPetInfo = NULL;
    DWORD KeepDays = 0;

    PacketSize_t getPacketSize() const {
        return szBYTE + pPetInfo->getSize() + szDWORD;
    }
    static constexpr PacketSize_t getPacketMaxSize() {
        return szBYTE + PetInfo::getMaxSize() + szDWORD;
    }
};

//////////////////////////////////////////////////////////////////////////////
// class GCPetStashList;
//////////////////////////////////////////////////////////////////////////////

class GCPetStashList : public Packet {
public:
    GCPetStashList();
    virtual ~GCPetStashList();

    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_PET_STASH_LIST;
    }
    PacketSize_t getPacketSize() const;
    string getPacketName() const {
        return "GCPetStashList";
    }
    string toString() const;

public:
    vector<PetStashItemInfo*>& getPetStashItemInfos() {
        return m_PetStashItemInfos;
    }

private:
    vector<PetStashItemInfo*> m_PetStashItemInfos;
    // Set by read() alone: a setter installs a borrowed record and is for
    // a packet being filled, never one that was read.
    bool m_bOwnsPetInfos = false; // true once read() allocated the slots' pets
};


//////////////////////////////////////////////////////////////////////////////
// class GCPetStashListFactory;
//////////////////////////////////////////////////////////////////////////////

class GCPetStashListFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_PET_STASH_LIST;
    static constexpr std::string_view kName = "GCPetStashList";
    static constexpr PacketSize_t kMaxSize{PetStashItemInfo::getPacketMaxSize() * MAX_PET_STASH};

    Packet* createPacket() override {
        return new GCPetStashList();
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


#endif
