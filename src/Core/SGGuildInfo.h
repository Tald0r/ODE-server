//----------------------------------------------------------------------
//
// Filename    : SGGuildInfo.h
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

#ifndef __SG_GUILD_INFO_H__
#define __SG_GUILD_INFO_H__

// include files
#include <list>
#include <memory>

#include "GuildInfo2.h"
#include "GuildMemberInfo2.h"
#include "Packet.h"
#include "PacketFactory.h"

typedef list<GuildInfo2*> GuildInfoList2;
typedef list<GuildInfo2*>::const_iterator GuildInfoListConstItor2;

//----------------------------------------------------------------------
//
// class SGGuildInfo;
//
//----------------------------------------------------------------------

class SGGuildInfo : public Packet {
public:
    // constructor
    SGGuildInfo();

    // destructor
    ~SGGuildInfo() noexcept;

    void read(SocketInputStream& iStream);

    void write(SocketOutputStream& oStream) const;


    // get packet id
    PacketID_t getPacketID() const {
        return PACKET_SG_GUILD_INFO;
    }

    // get packet's body size
    PacketSize_t getPacketSize() const;

    // get packet name
    string getPacketName() const {
        return "SGGuildInfo";
    }

    // get packet's debug string
    string toString() const;

public:
    // get guild info list num
    WORD getGuildInfoListNum() const {
        return m_GuildInfoList.size();
    }

    // add GuildInfo
    // Takes ownership and enforces the factory's guild-count budget.
    // Refusal or allocation failure destroys the
    // incoming entry; ownership never returns to the caller.
    void addGuildInfo(GuildInfo2* pGuildInfo) {
        std::unique_ptr<GuildInfo2> owned(pGuildInfo);
        if (m_GuildInfoList.size() >= GuildInfo2::kMaxCount)
            throw InvalidProtocolException("too many guild infos");
        m_GuildInfoList.push_front(owned.get());
        owned.release();
    }

    // Transfer a completed batch without allocating or reversing its order.
    // Count refusal leaves both packets unchanged. Transferring to self is a no-op.
    void prependGuildInfosFrom(SGGuildInfo& source) {
        if (this == &source)
            return;
        if (m_GuildInfoList.size() + source.m_GuildInfoList.size() > GuildInfo2::kMaxCount)
            throw InvalidProtocolException("too many guild infos");
        m_GuildInfoList.splice(m_GuildInfoList.begin(), source.m_GuildInfoList);
    }

    // clear GuildInfoList
    void clearGuildInfoList();

    // pop front element in GuildInfoList
    GuildInfo2* popFrontGuildInfoList() {
        if (m_GuildInfoList.empty())
            return NULL;

        GuildInfo2* pGuildInfo = m_GuildInfoList.front();
        m_GuildInfoList.pop_front();
        return pGuildInfo;
    }

private:
    // guild list
    GuildInfoList2 m_GuildInfoList;
};


//////////////////////////////////////////////////////////////////////
//
// class SGGuildInfoFactory;
//
// Factory for SGGuildInfo
//
//////////////////////////////////////////////////////////////////////

class SGGuildInfoFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_SG_GUILD_INFO;
    static constexpr std::string_view kName = "SGGuildInfo";
    static constexpr PacketSize_t kMaxSize{szWORD + GuildInfo2::getMaxSize() * GuildInfo2::kMaxCount};

    // create packet
    Packet* createPacket() override {
        return new SGGuildInfo();
    }

    // get packet name
    string getPacketName() const override {
        return string(kName);
    }

    // get packet id
    PacketID_t getPacketID() const override {
        return kPacketID;
    }

    // get packet's max body size
    // *OPTIMIZATION HINT*
    PacketSize_t getPacketMaxSize() const override {
        return kMaxSize;
    }
};


//////////////////////////////////////////////////////////////////////
//
// class LGIncomingConnectionHandler;
//
//////////////////////////////////////////////////////////////////////

class SGGuildInfoHandler {
public:
    // execute packet's handler
    static void execute(SGGuildInfo* pPacket);
};

#endif
