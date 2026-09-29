//----------------------------------------------------------------------
//
// Filename    : PCVampireInfo.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "PCVampireInfo.h"

#include "WireString.h"

void PCVampireInfo::setShapeInfo(DWORD flag, Color_t color[VAMPIRE_COLOR_MAX]) {
    // For now only the vampire coat changes shape..
    // If other parts change later this has to be changed with PCSlayerInfo as a reference
    m_CoatType = flag; //(flag & 7);
    m_CoatColor = color[0];
}

//----------------------------------------------------------------------
// read data from socket input stream
//----------------------------------------------------------------------
void PCVampireInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    //--------------------------------------------------
    // read vampire name
    //--------------------------------------------------

    de::wire::readString(iStream, m_Name, {1, 20}, "Name");

    //--------------------------------------------------
    // read slot
    //--------------------------------------------------
    BYTE slot;
    iStream.read(slot);
    m_Slot = Slot(slot);

    //--------------------------------------------------
    // read Alignment
    //--------------------------------------------------
    iStream.read(m_Alignment);

    //--------------------------------------------------
    // read sex
    //--------------------------------------------------
    BYTE sex;
    iStream.read(sex);
    if (sex > (BYTE)MALE)
        throw InvalidProtocolException("sex out of range");

    m_Sex = Sex(sex);

    //--------------------------------------------------
    // read colors
    //--------------------------------------------------
    iStream.read(m_BatColor);
    iStream.read(m_SkinColor);

    //--------------------------------------------------
    // read Shape
    //--------------------------------------------------
    BYTE coatType;
    iStream.read(coatType);
    m_CoatType = (ItemType_t)coatType;
    iStream.read(m_CoatColor);

    //--------------------------------------------------
    // read attributes
    //--------------------------------------------------
    iStream.read(m_STR);
    iStream.read(m_DEX);
    iStream.read(m_INT);

    //--------------------------------------------------
    // read hp
    //--------------------------------------------------
    iStream.read(m_HP[ATTR_CURRENT]);
    iStream.read(m_HP[ATTR_MAX]);

    //--------------------------------------------------
    // read misc
    //--------------------------------------------------
    iStream.read(m_Level);
    iStream.read(m_Rank);
    iStream.read(m_Exp);

    //--------------------------------------------------
    // read Fame
    //--------------------------------------------------
    iStream.read(m_Fame);


    //--------------------------------------------------
    // read Bonus Point
    //--------------------------------------------------
    iStream.read(m_Bonus);
    iStream.read(m_AdvancementLevel);

    __END_CATCH
}

//----------------------------------------------------------------------
// write data to socket output stream
//----------------------------------------------------------------------
void PCVampireInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    //--------------------------------------------------
    // write vampire name
    //--------------------------------------------------
    de::wire::writeString(oStream, m_Name, {1, 20}, "Name");

    //--------------------------------------------------
    // write slot
    //--------------------------------------------------
    oStream.write((BYTE)m_Slot);

    //--------------------------------------------------
    // write Alignment
    //--------------------------------------------------
    oStream.write(m_Alignment);

    //--------------------------------------------------
    // write sex
    //--------------------------------------------------
    oStream.write((BYTE)m_Sex);

    //--------------------------------------------------
    // write colors
    //--------------------------------------------------
    oStream.write(m_BatColor);
    oStream.write(m_SkinColor);

    //--------------------------------------------------
    // write Shape
    //--------------------------------------------------
    BYTE coatType = (BYTE)m_CoatType;
    oStream.write(coatType);
    oStream.write(m_CoatColor);

    //--------------------------------------------------
    // write attributes
    //--------------------------------------------------
    oStream.write(m_STR);
    oStream.write(m_DEX);
    oStream.write(m_INT);

    //--------------------------------------------------
    // write hp
    //--------------------------------------------------
    oStream.write(m_HP[ATTR_CURRENT]);
    oStream.write(m_HP[ATTR_MAX]);

    //--------------------------------------------------
    // write misc
    //--------------------------------------------------
    oStream.write(m_Level);
    oStream.write(m_Rank);
    oStream.write(m_Exp);

    //--------------------------------------------------
    // read Fame
    //--------------------------------------------------
    oStream.write(m_Fame);


    //--------------------------------------------------
    // write Bonus Point
    //--------------------------------------------------
    oStream.write(m_Bonus);
    oStream.write(m_AdvancementLevel);

    __END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string PCVampireInfo::toString() const {
    StringStream msg;

    msg << "PCVampireInfo(" << "Name:" << m_Name << ",Level:" << (int)m_Level
        << ",Slot:" << nameOrNumber(Slot2String, m_Slot) << ",Alignment:" << m_Alignment
        << ",Sex:" << nameOrNumber(Sex2String, m_Sex) << ",BatColor:" << (int)m_BatColor
        << ",SkinColor:" << (int)m_SkinColor << ",CoatType:" << (int)m_CoatType << ",CoatColor:" << (int)m_CoatColor
        << ",STR[BASIC]:" << (int)m_STR << ",DEX[BASIC]:" << (int)m_DEX << ",INT[BASIC]:" << (int)m_INT
        << ",HP:" << m_HP[ATTR_CURRENT] << "/" << m_HP[ATTR_MAX] << ",Rank:" << m_Rank << ",Exp:"
        << m_Exp
        //		<< ",Gold:" << m_Gold
        << ",Fame:"
        << m_Fame
        //		<< ",ZoneID:" << m_ZoneID
        << ")";

    return msg.toString();
}
