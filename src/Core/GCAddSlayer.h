//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddSlayer.h
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_ADD_SLAYER_H__
#define __GC_ADD_SLAYER_H__

#include "EffectInfo.h"
#include "NicknameInfo.h"
#include "PCSlayerInfo3.h"
#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"
#include "StoreInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCAddSlayer;
// Packet sent when a Slayer enters view. If a player already knows about the
// Slayer (including itself) and it only moves, GCMove is used. When a Slayer
// is seen for the first time or appears after a move to another map, send
// GCAddSlayer so the client can build the full Slayer state.
//////////////////////////////////////////////////////////////////////////////

class GCAddSlayer : public Packet {
public:
    GCAddSlayer();
    GCAddSlayer(const PCSlayerInfo3& slayerInfo);
    virtual ~GCAddSlayer() noexcept;

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_ADD_SLAYER;
    }
    PacketSize_t getPacketSize() const;
    string getPacketName() const {
        return "GCAddSlayer";
    }
    string toString() const;

public:
    PCSlayerInfo3& getSlayerInfo() {
        return m_SlayerInfo;
    }
    const PCSlayerInfo3& getSlayerInfo() const {
        return m_SlayerInfo;
    }
    void setSlayerInfo(const PCSlayerInfo3& slayerInfo) {
        m_SlayerInfo = slayerInfo;
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
    PCSlayerInfo3 m_SlayerInfo;    // Core Slayer info
    EffectInfo* m_pEffectInfo;     // Active effect info
    PetInfo* m_pPetInfo;           // Pet information
    NicknameInfo* m_pNicknameInfo; // Nickname information
    // Set by read() alone: a setter installs a borrowed record and is for
    // a packet being filled, never one that was read.
    bool m_bOwnsRecords = false; // true once read() allocated the pet and nickname
    // Frees the pet and nickname records when this packet owns them, then
    // forgets them either way.
    void releaseRecords();
    StoreOutlook m_StoreOutlook; // Store visual state
};

//////////////////////////////////////////////////////////////////////////////
// class GCAddSlayerFactory;
//////////////////////////////////////////////////////////////////////////////

class GCAddSlayerFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_ADD_SLAYER;
    static constexpr std::string_view kName = "GCAddSlayer";
    static constexpr PacketSize_t kMaxSize{PCSlayerInfo3::getMaxSize() + EffectInfo::getMaxSize() +
                                           PetInfo::getMaxSize() + NicknameInfo::getMaxSize() +
                                           StoreOutlook::getMaxSize()};

    Packet* createPacket() override {
        return new GCAddSlayer();
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
