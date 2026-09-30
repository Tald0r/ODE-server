//////////////////////////////////////////////////////////////////////////////
// Filename    : GameFrameGate.cpp
//////////////////////////////////////////////////////////////////////////////

#include "GameFrameGate.h"

#include <cstring>

#include "Exception.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "SocketInputStream.h"

namespace de {

namespace {

GameFrame refused(GameFrame frame, GameFrameRefusal refusal) {
    frame.step = GameFrameStep::Refused;
    frame.refusal = refusal;
    return frame;
}

} // namespace

GameFrame GameFrameGate::next(SocketInputStream& in, PlayerStatus status, PacketFactoryManager& factories,
                              PacketValidator& validator) {
    GameFrame frame;

    char header[szPacketHeader];
    if (!in.peek(&header[0], szPacketHeader)) {
        frame.step = GameFrameStep::AwaitHeader;
        return frame;
    }

    // The size counts the body only.
    std::memcpy(&frame.id, &header[0], szPacketID);
    std::memcpy(&frame.size, &header[szPacketID], szPacketSize);
    std::memcpy(&frame.sequence, &header[szPacketID + szPacketSize], szSequenceSize);

    if (frame.sequence != m_Sequence)
        return refused(std::move(frame), GameFrameRefusal::OutOfSequence);
    m_Sequence++;

    if (frame.id >= (int)Packet::PACKET_MAX)
        return refused(std::move(frame), GameFrameRefusal::IdOutOfRange);

    // A status whose set is PIST_IGNORE_EXCEPT answers an id outside the
    // set with IgnorePacketException rather than false. Nothing else on
    // this path throws it: readPacket turns one from a packet's read()
    // into an InvalidProtocolException.
    bool admitted = false;
    try {
        admitted = validator.isValidPacketID(status, frame.id);
    } catch (IgnorePacketException&) {
        // The frame is dropped unread, whole, once all of it has arrived.
        if (frame.size > factories.getPacketMaxSize(frame.id))
            return refused(std::move(frame), GameFrameRefusal::IgnoredTooLarge);
        if (in.length() < szPacketHeader + frame.size) {
            frame.step = GameFrameStep::AwaitIgnoredBody;
            return frame;
        }
        in.skip(szPacketHeader + frame.size);
        frame.step = GameFrameStep::Skipped;
        return frame;
    }
    if (!admitted)
        return refused(std::move(frame), GameFrameRefusal::InvalidOrder);

    // The store UI's info packets, which the validator refuses too.
    if (frame.id == Packet::PACKET_GC_OTHER_STORE_INFO || frame.id == Packet::PACKET_GC_MY_STORE_INFO)
        return refused(std::move(frame), GameFrameRefusal::StoreInfo);

    // An id with no factory throws InvalidProtocolException from here.
    if (frame.size > factories.getPacketMaxSize(frame.id))
        return refused(std::move(frame), GameFrameRefusal::TooLarge);

    if (in.length() < szPacketHeader + frame.size) {
        frame.step = GameFrameStep::AwaitBody;
        return frame;
    }

    frame.packet.reset(factories.createPacket(frame.id));
    in.readPacket(frame.packet.get());
    frame.step = GameFrameStep::Read;
    return frame;
}

} // namespace de
