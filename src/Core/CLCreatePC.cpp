//////////////////////////////////////////////////////////////////////////////
// Filename    : CLCreatePC.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CLCreatePC.h"

#include "WireString.h"

void CLCreatePC::read(SocketInputStream& iStream)

{
    __BEGIN_TRY

    de::wire::readString(iStream, m_Name, {1, 20}, "Name");

    BYTE slot;
    iStream.read(slot);

    // SLOT_MAX is the count of slots, not a slot, and Slot2String has one
    // entry per real slot. The byte is checked before it becomes a Slot:
    // an enum object holding a value outside its enumeration is undefined
    // to load, so a guard placed after the assignment could not be reached
    // with the input it exists to refuse.
    if (slot >= (BYTE)SLOT_MAX)
        throw InvalidProtocolException("slot out of range");

    m_Slot = Slot(slot);

    BYTE flags;
    iStream.read(flags);

    // Two bits of the flags byte carry the hair style, which reaches 3 while
    // HairStyle stops at HAIR_STYLE3 (the enum has no count enumerator) and
    // HairStyle2String has three entries. Bit SLAYER_BIT_SEX is a single bit
    // and names MALE or FEMALE either way, so it needs no check. Bits above
    // the three-bit set do not survive the assignment below.
    const BYTE hairStyle = (flags >> SLAYER_BIT_HAIRSTYLE) & 3;
    if (hairStyle > (BYTE)HAIR_STYLE3)
        throw InvalidProtocolException("hair style out of range");

    m_BitSet = flags;

    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        iStream.read(m_Colors[i]);

    iStream.read(m_STR);
    iStream.read(m_DEX);
    iStream.read(m_INT);

    iStream.read(m_Race);

    __END_CATCH
}

void CLCreatePC::write(SocketOutputStream& oStream) const

{
    __BEGIN_TRY

    de::wire::writeString(oStream, m_Name, {1, 20}, "Name");

    oStream.write((BYTE)m_Slot);

    oStream.write((BYTE)m_BitSet.to_ulong());

    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        oStream.write(m_Colors[i]);

    oStream.write(m_STR);
    oStream.write(m_DEX);
    oStream.write(m_INT);

    oStream.write(m_Race);

    __END_CATCH
}

string CLCreatePC::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "CLCreatePC(Name: " << m_Name << ",Slot:" << nameOrNumber(Slot2String, m_Slot)
        << ",Sex:" << nameOrNumber(Sex2String, getSex())
        << ",HairStyle:" << nameOrNumber(HairStyle2String, getHairStyle()) << ",HairColor:" << (int)getHairColor()
        << ",SkinColor:" << (int)getSkinColor() << ",STR:" << (int)m_STR << ",DEX:" << (int)m_DEX
        << ",INT:" << (int)m_INT << ",Race:" << (int)m_Race << ")";
    return msg.toString();

    __END_CATCH
}
