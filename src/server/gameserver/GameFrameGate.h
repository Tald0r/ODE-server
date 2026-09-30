//////////////////////////////////////////////////////////////////////////////
// Filename    : GameFrameGate.h
// Description : What GamePlayer::processCommand does with the frame at the
//               front of a game client's input stream: wait for more bytes,
//               read the packet, skip a frame the validator ignores, or
//               refuse it. Separated from GamePlayer so the gate can be
//               exercised with an in-memory stream and no player, creature
//               or socket.
//////////////////////////////////////////////////////////////////////////////

#ifndef __GAME_FRAME_GATE_H__
#define __GAME_FRAME_GATE_H__

#include <memory>

#include "Packet.h"
#include "PlayerStatus.h"

class PacketFactoryManager;
class PacketValidator;
class SocketInputStream;

namespace de {

// What the gate did with the frame at the front of the stream.
enum class GameFrameStep {
    // Fewer than szPacketHeader bytes are buffered.
    AwaitHeader,
    // An admitted frame whose body has not all arrived. Nothing was
    // consumed and the sequence was not counted.
    AwaitBody,
    // A frame the validator ignores in this status, whose body has not all
    // arrived. Nothing was consumed and the sequence was not counted.
    AwaitIgnoredBody,
    // An admitted frame, consumed and read into GameFrame::packet.
    Read,
    // A frame the validator ignores in this status, consumed unread.
    Skipped,
    // The frame breaks the protocol and the connection is to be dropped;
    // GameFrame::refusal says why. The frame is left in the stream, but
    // for an ignored frame refused for its sequence, which was skipped.
    Refused,
};

// Why the gate refused a frame. For an admitted frame the kinds are listed
// in the order the gate checks them, IgnoredTooLarge aside: the header's
// id, the validator and the declared size are checked as soon as the
// header has arrived, the sequence byte only once the whole frame has,
// just before it is consumed. A frame the validator ignores is checked
// for IdOutOfRange, then IgnoredTooLarge, then, once it has been skipped,
// OutOfSequence. A refused frame's sequence is not counted, which changes
// nothing, since a refusal drops the connection.
enum class GameFrameRefusal {
    None,
    // The id is at or past Packet::PACKET_MAX.
    IdOutOfRange,
    // The validator does not admit the id in the player's status.
    InvalidOrder,
    // GCOtherStoreInfo or GCMyStoreInfo, which a client never sends.
    StoreInfo,
    // The declared body is larger than the packet's maximum.
    TooLarge,
    // As TooLarge, for a frame the validator ignores.
    IgnoredTooLarge,
    // The header's sequence byte is not the one the gate expects next.
    OutOfSequence,
};

struct GameFrame {
    GameFrameStep step = GameFrameStep::AwaitHeader;
    GameFrameRefusal refusal = GameFrameRefusal::None;
    // The header's fields; meaningless on AwaitHeader.
    PacketID_t id = 0;
    PacketSize_t size = 0;
    SequenceSize_t sequence = 0;
    // The packet read, on Read only.
    std::unique_ptr<Packet> packet;
};

// How a refused frame ends the connection. GamePlayer::processCommand
// writes its log line, whose fields name the player, to
// refusalLogFile(refusal), then calls throwRefusal(refusal): an
// out-of-sequence frame throws DisconnectException("Packet sequence
// error"), every other refusal an InvalidProtocolException naming it.
const char* refusalLogFile(GameFrameRefusal refusal);
[[noreturn]] void throwRefusal(GameFrameRefusal refusal);

// The receive loop's per-connection gate. It owns the count of frames the
// client has sent, which the client numbers one by one in each header's
// sequence byte, starting at 0 and wrapping at 256.
class GameFrameGate {
public:
    // Decides the frame at the front of `in` for a player in `status` and,
    // when the frame is admitted and whole, consumes it: an admitted frame
    // is read with SocketInputStream::readPacket, an ignored one skipped.
    // The sequence byte is checked and counted only then, where the frame
    // is consumed: a frame that waits for the rest of its body is left in
    // the stream uncounted and decided afresh on the next call, so the
    // result does not depend on where the network cut the bytes. An
    // ignored frame's sequence is checked too, once it has been skipped.
    // A ProtocolException from the factory table (an id with no factory)
    // or from readPacket (a malformed body) is passed on.
    GameFrame next(SocketInputStream& in, PlayerStatus status, PacketFactoryManager& factories,
                   PacketValidator& validator);

    // The sequence byte the next frame must carry.
    SequenceSize_t expectedSequence() const {
        return m_Sequence;
    }

private:
    // Checks a consumed frame's sequence byte and counts it; false when
    // it is out of step.
    bool countSequence(SequenceSize_t sequence);

    SequenceSize_t m_Sequence = 0;
};

} // namespace de

#endif // __GAME_FRAME_GATE_H__
