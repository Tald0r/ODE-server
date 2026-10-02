//////////////////////////////////////////////////////////////////////
//
// Filename    : LCServerList.h
// Written By  : Reiot
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __LC_SERVER_LIST_H__
#define __LC_SERVER_LIST_H__

// include files
#include <memory>

#include "Packet.h"
#include "PacketFactory.h"
#include "ServerGroupInfo.h"

//////////////////////////////////////////////////////////////////////
//
// class LCServerList;
//
//////////////////////////////////////////////////////////////////////

class LCServerList : public Packet {
public:
    // constructor
    // Set every entry of the PCInfo* array to NULL.
    LCServerList();

    // destructor
    // Delete the objects allocated in the PCInfo* array.
    ~LCServerList() noexcept;

    // Read data from the input stream (buffer) and initialise the
    // packet.
    void read(SocketInputStream& iStream);

    // Send the packet's binary image to the output stream (buffer).
    void write(SocketOutputStream& oStream) const;


    // get packet id
    PacketID_t getPacketID() const {
        return PACKET_LC_SERVER_LIST;
    }

    // get packet's body size
    PacketSize_t getPacketSize() const;

    // get packet's name
    string getPacketName() const {
        return "LCServerList";
    }

    // get packet's debug string
    string toString() const;

public:
    // Current server group
    ServerGroupID_t getCurrentServerGroupID() const {
        return m_CurrentServerGroupID;
    }
    void setCurrentServerGroupID(ServerGroupID_t ServerGroupID) {
        m_CurrentServerGroupID = ServerGroupID;
    }

    BYTE getListNum() const {
        return m_ServerGroupInfoList.size();
    }

    // add / delete / clear S List
    // Takes ownership and enforces the factory's count budget. Count refusal
    // and failed list allocation both destroy the incoming entry.
    void addListElement(ServerGroupInfo* pServerGroupInfo) {
        std::unique_ptr<ServerGroupInfo> owned(pServerGroupInfo);
        if (m_ServerGroupInfoList.size() >= ServerGroupInfo::kMaxCount)
            throw InvalidProtocolException("too many server group infos");
        m_ServerGroupInfoList.push_back(owned.get());
        owned.release();
    }

    // ClearList
    void clearList() noexcept {
        while (!m_ServerGroupInfoList.empty()) {
            delete m_ServerGroupInfoList.front();
            m_ServerGroupInfoList.pop_front();
        }
    }

    // pop front Element in Status List
    ServerGroupInfo* popFrontListElement() {
        ServerGroupInfo* TempServerGroupInfo = m_ServerGroupInfoList.front();
        m_ServerGroupInfoList.pop_front();
        return TempServerGroupInfo;
    }

private:
    // Current server group
    ServerGroupID_t m_CurrentServerGroupID;

    // Character information
    list<ServerGroupInfo*> m_ServerGroupInfoList;
};

//////////////////////////////////////////////////////////////////////
//
// class LCServerListFactory;
//
// Factory for LCServerList
//
//////////////////////////////////////////////////////////////////////

class LCServerListFactory : public PacketFactory {
public:
    static constexpr PacketID_t kPacketID = Packet::PACKET_LC_SERVER_LIST;
    static constexpr std::string_view kName = "LCServerList";
    // write() emits a BYTE ListNum between the group id and the infos
    static constexpr PacketSize_t kMaxSize{szServerGroupID + szBYTE + ServerGroupInfo::getMaxSize()};

    // create packet
    Packet* createPacket() override {
        return new LCServerList();
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
    PacketSize_t getPacketMaxSize() const override {
        return kMaxSize;
    }
};


//////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////

#endif
