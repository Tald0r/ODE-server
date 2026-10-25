//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddOusters.h
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_ADD_OUSTERS_H__
#define __GC_ADD_OUSTERS_H__

#include "EffectInfo.h"
#include "NicknameInfo.h"
#include "PCOustersInfo3.h"
#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"
#include "StoreInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCAddOusters;
// When a slayer newly enters a zone through login, a portal or a teleport, or
// when a slayer moves within a zone,(1) the PCs in the area that already hold
// information about this slayer (that is, that can see it) get the GCMove packet
// broadcast to them. But,(2) the PCs in the area that see this slayer
// for the first time get the GCAddOusters packet broadcast to them. Also,(3)
// this slayer receives, inside GCAddOusters, the information about the
// slayers within its newly opened field of view.
//////////////////////////////////////////////////////////////////////////////

class GCAddOusters : public Packet {
public:
    GCAddOusters();
    GCAddOusters(const PCOustersInfo3& slayerInfo);
    virtual ~GCAddOusters();

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_ADD_OUSTERS;
    }
    PacketSize_t getPacketSize() const;
    string getPacketName() const {
        return "GCAddOusters";
    }
    string toString() const;

public:
    PCOustersInfo3& getOustersInfo() {
        return m_OustersInfo;
    }
    const PCOustersInfo3& getOustersInfo() const {
        return m_OustersInfo;
    }
    void setOustersInfo(const PCOustersInfo3& slayerInfo) {
        m_OustersInfo = slayerInfo;
    }

    EffectInfo* getEffectInfo() const {
        return m_pEffectInfo;
    }
    void setEffectInfo(EffectInfo* pEffectInfo) {
        m_pEffectInfo = pEffectInfo;
    }

    PetInfo* getPetInfo() const {
        return m_pPetInfo;
    }
    void setPetInfo(PetInfo* pPetInfo) {
        m_pPetInfo = pPetInfo;
    }

    NicknameInfo* getNicknameInfo() const {
        return m_pNicknameInfo;
    }
    void setNicknameInfo(NicknameInfo* pNicknameInfo) {
        m_pNicknameInfo = pNicknameInfo;
    }

    StoreOutlook getStoreOutlook() const {
        return m_StoreOutlook;
    }
    void setStoreInfo(StoreInfo* pInfo) {
        pInfo->makeStoreOutlook(m_StoreOutlook);
    }

private:
    PCOustersInfo3 m_OustersInfo; // Slayer's appearance information
    EffectInfo* m_pEffectInfo;    // Information about the effects in force
    PetInfo* m_pPetInfo;          // Information about the pet
    NicknameInfo* m_pNicknameInfo;
    // Set by read() alone: a setter installs a borrowed record and is for
    // a packet being filled, never one that was read.
    bool m_bOwnsRecords = false; // true once read() allocated the pet and nickname
    // Frees the effect record, and the pet and nickname records when this
    // packet owns them; forgets them all either way.
    void releaseRecords();
    StoreOutlook m_StoreOutlook;
};

//////////////////////////////////////////////////////////////////////////////
// class GCAddOustersFactory;
//////////////////////////////////////////////////////////////////////////////

class GCAddOustersFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_ADD_OUSTERS;
    static constexpr std::string_view kName = "GCAddOusters";
    static constexpr PacketSize_t kMaxSize{PCOustersInfo3::getMaxSize() + EffectInfo::getMaxSize() +
                                           PetInfo::getMaxSize() + NicknameInfo::getMaxSize() +
                                           StoreOutlook::getMaxSize()};

    Packet* createPacket() override {
        return new GCAddOusters();
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
