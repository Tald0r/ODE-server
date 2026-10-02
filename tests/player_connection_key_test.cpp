#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#include <gtest/gtest.h>

#include "ConnectionKey.h"
#include "Player.h"
#include "Socket.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "support/AllocationProbe.h"
#include "support/LoopbackListener.h"
#include "support/SocketInputStreamTestAccess.h"

namespace {
class KeyPlayer : public Player {
public:
    using Player::Player;
    KeyPlayer() = default;

    explicit KeyPlayer(int mask) : Player(new Socket(), mask & 1 ? 64 : 0, mask & 2 ? 64 : 0) {}

    const de::ConnectionKey* key() const {
        return m_ConnectionKey.get();
    }

    SocketInputStream& input() {
        return *m_pInputStream;
    }
    SocketOutputStream& output() {
        return *m_pOutputStream;
    }
};

std::uint64_t tableDigest(const de::ConnectionKey& key) {
    std::uint64_t digest = 14695981039346656037ull;
    for (BYTE byte : key.table)
        digest = (digest ^ byte) * 1099511628211ull;
    return digest;
}

TEST(ConnectionKey, PreservesLegacyTableVectorsAndNormalizesOffsets) {
    // Recorded from the original generator. Each digest covers all 512 bytes.
    struct Vector {
        WORD encryptKey;
        WORD hashKey;
        WORD offset;
        std::uint64_t digest;
    };
    constexpr Vector vectors[] = {
        {0, 0x0000, 0, 0xbee8383a4a575325ull},        {511, 0x0001, 511, 0xc63b34d53cfcab25ull},
        {512, 0x0002, 0, 0x442262a3c0407b25ull},      {513, 0x00ff, 1, 0xd38fc98d07ce1b25ull},
        {0xffff, 0x1234, 511, 0x6b6b1ceda491ab25ull}, {0xaeb7, 0x9b3e, 183, 0x13a160d954686325ull},
        {1023, 0xabcd, 511, 0x387a5273eb6b5325ull},   {1024, 0xffff, 0, 0xd38fc98d07ce1b25ull},
    };
    for (const auto& vector : vectors) {
        SCOPED_TRACE(vector.hashKey);
        const auto key = de::makeConnectionKey(vector.encryptKey, vector.hashKey);
        EXPECT_EQ(vector.offset, key.offset);
        EXPECT_EQ(vector.digest, tableDigest(key));
    }
}

TEST(ConnectionKey, HashHighBytesDoNotChangeTheTableAndCalculationDoesNotAllocate) {
    ASSERT_EXIT(
        {
            AllocationProbe probe(1);
            for (unsigned low = 0; low < 256; ++low) {
                const auto expected = de::makeConnectionKey(0, low);
                for (unsigned high = 0; high < 256; ++high) {
                    const auto key = de::makeConnectionKey(0xffff, (high << 8) | low);
                    if (key.offset != 511 || key.table != expected.table)
                        std::_Exit(1);
                }
            }
            std::_Exit(probe.attempts() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerConnectionKey, DefaultPlayerStartsEmptyAndCanOwnAKeyWithoutStreams) {
    ASSERT_EXIT(
        {
            AllocationProbe probe;
            {
                KeyPlayer player;
                if (player.key() || player.getSocket())
                    std::_Exit(1);
                player.setKey(0xffff, 0x9b3e);
                if (!player.key() || player.key()->offset != 511 || tableDigest(*player.key()) != 0x13a160d954686325ull)
                    std::_Exit(2);
            }
            std::_Exit(probe.outstanding() == 0 ? 0 : 3);
        },
        ::testing::ExitedWithCode(0), "");
}

class KeyStreamModes : public ::testing::TestWithParam<int> {};

TEST_P(KeyStreamModes, InitialAndRepeatedInstallationPreserveStateOnFailureAndCanRetry) {
    const int mask = GetParam();
    for (bool installed : {false, true}) {
        SCOPED_TRACE(installed);
        for (std::size_t failAt = 1; failAt <= 2; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(
                {
                    auto player = std::make_unique<KeyPlayer>(mask);
                    if (player->key())
                        std::_Exit(1);
                    if (installed)
                        player->setKey(513, 0x1234);
                    const auto* previous = player->key();
                    auto previousBytes = de::ConnectionKey{}.table;
                    if (previous)
                        previousBytes = previous->table;
                    const unsigned char buffered = 'i';
                    if (mask & 1) {
                        if (!SocketInputStreamTestAccess::Preload(player->input(), &buffered, 1))
                            std::_Exit(2);
                        player->input().m_EncryptKey = 31;
                    }
                    if (mask & 2) {
                        player->output().write("o", 1);
                        player->output().m_EncryptKey = 47;
                    }
                    AllocationProbe probe(failAt);
                    bool threw = false;
                    try {
                        player->setKey(19, 0xabcd);
                    } catch (const std::bad_alloc&) {
                        threw = true;
                    }
                    if (threw != probe.rejected())
                        std::_Exit(3);
                    if (threw) {
                        if (player->key() != previous || probe.outstanding() != 0 ||
                            (previous && previous->table != previousBytes))
                            std::_Exit(4);
                        const auto* borrowed = previous ? previous->table.data() : nullptr;
                        if ((mask & 1) &&
                            (player->input().m_HashTable != borrowed || player->input().m_EncryptKey != 31))
                            std::_Exit(5);
                        if ((mask & 2) &&
                            (player->output().m_HashTable != borrowed || player->output().m_EncryptKey != 47))
                            std::_Exit(6);
                        probe.stopFailing();
                        player->setKey(19, 0xabcd);
                    }
                    if (!player->key() || tableDigest(*player->key()) != 0x387a5273eb6b5325ull)
                        std::_Exit(7);
                    if (mask & 1) {
                        char received;
                        if (player->input().m_HashTable != player->key()->table.data() ||
                            player->input().m_EncryptKey != 19 || player->input().read(&received, 1) != 1 ||
                            received != 'i')
                            std::_Exit(8);
                    }
                    if ((mask & 2) && (player->output().m_HashTable != player->key()->table.data() ||
                                       player->output().m_EncryptKey != 19 || player->output().length() != 1 ||
                                       player->output().getBuffer()[0] != 'o'))
                        std::_Exit(9);
                    player.reset();
                    std::_Exit(probe.outstanding() == 0 && (failAt != 2 || !probe.rejected()) ? 0 : 10);
                },
                ::testing::ExitedWithCode(0), "");
        }
    }
}

INSTANTIATE_TEST_SUITE_P(StreamModes, KeyStreamModes, ::testing::Values(0, 1, 2, 3));

TEST(PlayerConnectionKey, SocketReplacementDropsOldKeysOnlyAfterTheNewStreamsAreReady) {
    for (bool sameSocket : {false, true}) {
        SCOPED_TRACE(sameSocket);
        for (std::size_t failAt = 1; failAt <= 5; ++failAt) {
            SCOPED_TRACE(failAt);
            ASSERT_EXIT(
                {
                    auto socket = std::make_unique<Socket>();
                    auto player = std::unique_ptr<KeyPlayer>(new KeyPlayer(socket.release()));
                    auto* previousSocket = player->getSocket();
                    player->setKey(513, 0x1234);
                    const auto* previousKey = player->key();
                    auto replacement = sameSocket ? nullptr : std::make_unique<Socket>();
                    AllocationProbe probe(failAt);
                    bool threw = false;
                    try {
                        player->setSocket(sameSocket ? previousSocket : replacement.release());
                    } catch (const std::bad_alloc&) {
                        threw = true;
                    }
                    if (threw != probe.rejected())
                        std::_Exit(1);
                    if (threw) {
                        if (player->key() != previousKey || player->getSocket() != previousSocket ||
                            player->input().m_HashTable != previousKey->table.data() ||
                            player->output().m_HashTable != previousKey->table.data() ||
                            player->input().m_EncryptKey != 1 || player->output().m_EncryptKey != 1 ||
                            probe.outstanding() != 0)
                            std::_Exit(2);
                        probe.stopFailing();
                        auto retry = sameSocket ? nullptr : std::make_unique<Socket>();
                        player->setSocket(sameSocket ? previousSocket : retry.release());
                    }
                    if (player->key() || player->input().m_HashTable || player->output().m_HashTable ||
                        player->input().m_EncryptKey != 0 || player->output().m_EncryptKey != 0)
                        std::_Exit(3);
                    probe.stopFailing();
                    player->setKey(7, 0x9b3e);
                    if (player->input().m_HashTable != player->output().m_HashTable ||
                        player->input().m_EncryptKey != 7 || player->output().m_EncryptKey != 7)
                        std::_Exit(4);
                    player.reset();
                    std::_Exit(probe.outstanding() == 0 && (failAt != 5 || !probe.rejected()) ? 0 : 5);
                },
                ::testing::ExitedWithCode(0), "");
        }
    }
}

bool readable(int descriptor) {
    pollfd ready{descriptor, POLLIN, 0};
    return ::poll(&ready, 1, 2000) == 1 && (ready.revents & (POLLIN | POLLHUP));
}

TEST(PlayerConnectionKey, InstalledKeysPreservePlainStreamBytesInBothDirections) {
    ASSERT_EXIT(
        {
            LoopbackListener listener;
            auto socket = std::make_unique<Socket>("127.0.0.1", listener.port());
            socket->connect();
            socket->setNonBlocking(true);
            const int peer = listener.accept();
            KeyPlayer player(socket.release());
            player.setKey(513, 0xabcd);
            const char bytes[] = "a\0b\xff";
            char received[sizeof(bytes)]{};
            if (::send(peer, bytes, sizeof(bytes), 0) != sizeof(bytes))
                std::_Exit(1);
            while (player.input().length() < sizeof(bytes)) {
                if (!readable(player.getSocket()->getSOCKET()))
                    std::_Exit(2);
                player.processInput();
            }
            if (player.input().read(received, sizeof(received)) != sizeof(received) ||
                std::memcmp(bytes, received, sizeof(bytes)) != 0 || player.input().m_EncryptKey != 1)
                std::_Exit(3);
            player.output().write(bytes, sizeof(bytes));
            player.processOutput();
            for (std::size_t offset = 0; offset < sizeof(received);) {
                if (!readable(peer))
                    std::_Exit(4);
                const auto count = ::recv(peer, received + offset, sizeof(received) - offset, 0);
                if (count <= 0)
                    std::_Exit(5);
                offset += count;
            }
            const bool intact = std::memcmp(bytes, received, sizeof(bytes)) == 0 && player.output().m_EncryptKey == 1;
            ::close(peer);
            std::_Exit(intact ? 0 : 6);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerConnectionKey, PlayerDestructionReleasesTheInstalledTable) {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<KeyPlayer>(new KeyPlayer(socket.release()));
            AllocationProbe probe;
            player->setKey(513, 0x1234);
            player.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerConnectionKey, ReplacingKeysKeepsOnlyTheCurrentTableAlive) {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<KeyPlayer>(new KeyPlayer(socket.release()));
            AllocationProbe probe;
            for (WORD key = 1; key <= 3; ++key) {
                player->setKey(key, key + 1);
                if (probe.outstanding() != 1 || player->input().m_HashTable != player->output().m_HashTable ||
                    player->input().m_EncryptKey != key || player->output().m_EncryptKey != key)
                    std::_Exit(1);
            }
            player.reset();
            std::_Exit(probe.outstanding() == 0 ? 0 : 2);
        },
        ::testing::ExitedWithCode(0), "");
}

TEST(PlayerConnectionKey, AReceivedKeyPairDoesNotExitTheServerProcess) {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            KeyPlayer player(socket.release());
            player.setKey(513, 0x1234);
            player.setKey(0xAEB7, 0x9B3E);
            std::_Exit(42);
        },
        ::testing::ExitedWithCode(42), "");
}

TEST(PlayerConnectionKey, AllocationFailurePreservesBothStreamsAndTheirPreviousKeyTable) {
    ASSERT_EXIT(
        {
            auto socket = std::make_unique<Socket>();
            auto player = std::unique_ptr<KeyPlayer>(new KeyPlayer(socket.release()));
            player->setKey(513, 0x1234);
            auto* previous = player->input().m_HashTable;
            const BYTE first = previous[0];
            const BYTE last = previous[511];
            AllocationProbe probe(1);
            bool threw = false;
            try {
                player->setKey(17, 0x9876);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
            const bool intact = threw && probe.rejected() && probe.outstanding() == 0 &&
                                player->input().m_HashTable == previous && player->output().m_HashTable == previous &&
                                player->input().m_EncryptKey == 1 && player->output().m_EncryptKey == 1 &&
                                previous[0] == first && previous[511] == last;
            player.reset();
            std::_Exit(intact ? 0 : 1);
        },
        ::testing::ExitedWithCode(0), "");
}
} // namespace
