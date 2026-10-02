#ifndef DARKEDEN_TEST_CONNECTION_KEY_DISPATCH_CHECKS_H
#define DARKEDEN_TEST_CONNECTION_KEY_DISPATCH_CHECKS_H

#include <cstdlib>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "AllocationProbe.h"
#include "CGConnectSetKey.h"
#include "PacketDispatcher.h"
#include "Player.h"
#include "Socket.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

namespace connection_key_test {
class InspectedPlayer : public Player {
public:
    using Player::Player;
    SocketInputStream& input() {
        return *m_pInputStream;
    }
    SocketOutputStream& output() {
        return *m_pOutputStream;
    }
};

// Called only after the runtime fixture registers its production dispatch
// table. Both servers must run the same ownership contract through their own
// handler objects, compiled with their production definitions.
inline void checkDispatchedReplacement() {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<InspectedPlayer>(new InspectedPlayer(socket.release()));
            AllocationProbe probe;
            CGConnectSetKey packet;
            packet.setEncryptKey(513);
            packet.setHashKey(0x1234);
            PacketDispatcher::dispatch(&packet, player.get());
            if (!player->input().m_HashTable || player->input().m_HashTable != player->output().m_HashTable ||
                player->input().m_EncryptKey != 1 || player->output().m_EncryptKey != 1)
                std::_Exit(1);
            packet.setEncryptKey(0xAEB7);
            packet.setHashKey(0x9B3E);
            PacketDispatcher::dispatch(&packet, player.get());
            if (probe.outstanding() != 1 || !player->input().m_HashTable ||
                player->input().m_HashTable != player->output().m_HashTable || player->input().m_EncryptKey != 183 ||
                player->output().m_EncryptKey != 183)
                std::_Exit(2);
            player.reset();
            std::_Exit(probe.outstanding() == 0 ? 42 : 3);
        },
        ::testing::ExitedWithCode(42), "");
}

inline void checkDispatchedFailureAndRetry() {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<InspectedPlayer>(new InspectedPlayer(socket.release()));
            CGConnectSetKey packet;
            packet.setEncryptKey(0xFFFF);
            packet.setHashKey(0x1234);
            PacketDispatcher::dispatch(&packet, player.get());
            const auto* previous = player->input().m_HashTable;
            AllocationProbe probe(1);
            packet.setEncryptKey(19);
            packet.setHashKey(0xABCD);
            bool threw = false;
            try {
                PacketDispatcher::dispatch(&packet, player.get());
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            if (!threw || !probe.rejected() || probe.outstanding() != 0 || player->input().m_HashTable != previous ||
                player->output().m_HashTable != previous || player->input().m_EncryptKey != 511 ||
                player->output().m_EncryptKey != 511)
                std::_Exit(1);
            probe.stopFailing();
            PacketDispatcher::dispatch(&packet, player.get());
            if (!player->input().m_HashTable || player->input().m_HashTable == previous ||
                player->input().m_HashTable != player->output().m_HashTable || player->input().m_EncryptKey != 19 ||
                player->output().m_EncryptKey != 19)
                std::_Exit(2);
            player.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace connection_key_test

#endif
