//////////////////////////////////////////////////////////////////////////////
// Filename    : GCAddVampire.h
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __GC_ADD_VAMPIRE_H__
#define __GC_ADD_VAMPIRE_H__

#include "EffectInfo.h"
#include "NicknameInfo.h"
#include "PCVampireInfo3.h"
#include "Packet.h"
#include "PacketFactory.h"
#include "PetInfo.h"
#include "StoreInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class GCAddVampire;
//
// When a slayer newly enters a zone through login, a portal or a teleport, or
// when a slayer moves within a zone,(1) the PCs in the area that already hold
// information about this slayer (that is, that can see it) get the GCMove packet
// broadcast to them. But,(2) the PCs in the area that see this slayer
// for the first time get the GCAddVampire packet broadcast to them. Also,(3)
// this slayer receives, inside GCAddVampire, the information about the
// slayers within its newly opened field of view.
//////////////////////////////////////////////////////////////////////////////

class GCAddVampire : public Packet {
public:
    GCAddVampire() : m_pEffectInfo(NULL), m_pPetInfo(NULL), m_pNicknameInfo(NULL) {
        m_FromFlag = 0;
    }
    GCAddVampire(const PCVampireInfo3& vampireInfo)
        : m_VampireInfo(vampireInfo), m_pEffectInfo(NULL), m_pPetInfo(NULL), m_pNicknameInfo(NULL) {
        m_FromFlag = 0;
    }
    virtual ~GCAddVampire();

public:
    void read(SocketInputStream& iStream);
    void write(SocketOutputStream& oStream) const;
    PacketID_t getPacketID() const {
        return PACKET_GC_ADD_VAMPIRE;
    }
    PacketSize_t getPacketSize() const {
        EffectInfo noEffects;
        const EffectInfo& effects = (m_pEffectInfo != NULL) ? *m_pEffectInfo : noEffects;

        PacketSize_t ret = m_VampireInfo.getSize() + effects.getSize() +
                           ((m_pPetInfo != NULL) ? m_pPetInfo->getSize() : szPetType) + szBYTE;

        if (m_pNicknameInfo == NULL) {
            NicknameInfo noNick;
            noNick.setNicknameType(NicknameInfo::NICK_NONE);
            ret += noNick.getSize();
        } else {
            ret += m_pNicknameInfo->getSize();
        }

        ret += m_StoreOutlook.getSize();

        return ret;
    }
    string getPacketName() const {
        return "GCAddVampire";
    }
    string toString() const;

public:
    PCVampireInfo3& getVampireInfo() {
        return m_VampireInfo;
    }
    const PCVampireInfo3& getVampireInfo() const {
        return m_VampireInfo;
    }
    void setVampireInfo(const PCVampireInfo3& vampireInfo) {
        m_VampireInfo = vampireInfo;
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

    BYTE getFromFlag(void) const {
        return m_FromFlag;
    }
    void setFromFlag(BYTE flag) {
        m_FromFlag = flag;
    }

private:
    PCVampireInfo3 m_VampireInfo;  // Vampire's appearance information
    EffectInfo* m_pEffectInfo;     // Effect information
    PetInfo* m_pPetInfo;           // Pet information
    NicknameInfo* m_pNicknameInfo; // Pet information
    // Set by read() alone: a setter installs a borrowed record and is for
    // a packet being filled, never one that was read.
    bool m_bOwnsRecords = false; // true once read() allocated the pet and nickname
    // Frees the effect record, and the pet and nickname records when this
    // packet owns them; forgets them all either way.
    void releaseRecords();
    StoreOutlook m_StoreOutlook; // Personal store information
    BYTE m_FromFlag;             // Where from? 0 when normal, 1 when through a portal
};


//////////////////////////////////////////////////////////////////////////////
// class GCAddVampireFactory;
//////////////////////////////////////////////////////////////////////////////

class GCAddVampireFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_GC_ADD_VAMPIRE;
    static constexpr std::string_view kName = "GCAddVampire";
    static constexpr PacketSize_t kMaxSize{PCVampireInfo3::getMaxSize() + EffectInfo::getMaxSize() +
                                           PetInfo::getMaxSize() + NicknameInfo::getMaxSize() +
                                           StoreOutlook::getMaxSize() + szBYTE};

    Packet* createPacket() override {
        return new GCAddVampire();
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
