//----------------------------------------------------------------------
//
// Filename    : PCSlayerInfo.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "PCSlayerInfo.h"

#include "WireString.h"

//----------------------------------------------------------------------
// set ShapeInfo
//----------------------------------------------------------------------
void PCSlayerInfo::setShapeInfo(DWORD flag, Color_t color[SLAYER_COLOR_MAX]) {
    m_Outlook = bitset<SLAYER_BIT_MAX>(flag);


    for (uint i = 0; i < SLAYER_COLOR_MAX; i++) {
        m_Colors[i] = color[i];
    }
}

//----------------------------------------------------------------------
// read data from socket input stream
//----------------------------------------------------------------------
void PCSlayerInfo::read(SocketInputStream& iStream) {
    __BEGIN_TRY
    //--------------------------------------------------
    // read slayer name
    //--------------------------------------------------

    de::wire::readString(iStream, m_Name, {1, 20}, "Name");

    //--------------------------------------------------
    // read slot
    //
    // The real size of an enum variable and the size written to the stream are
    // different in most cases, so a variable of the size that is written
    // is used to read it instead, and it is converted back to the enum and
    // stored instead.
    //
    // ex> Slot, Sex, HairStyle ...
    //
    //--------------------------------------------------
    BYTE slot;
    iStream.read(slot);
    m_Slot = Slot(slot);

    //--------------------------------------------------
    // read Alignment
    //--------------------------------------------------
    iStream.read(m_Alignment);

    //--------------------------------------------------
    // read attributes
    //--------------------------------------------------
    iStream.read(m_STR);
    iStream.read(m_DEX);
    iStream.read(m_INT);

    //--------------------------------------------------
    // read attributes Exp
    //--------------------------------------------------
    iStream.read(m_Rank);

    iStream.read(m_STRExp);
    iStream.read(m_DEXExp);
    iStream.read(m_INTExp);

    //--------------------------------------------------
    // read hp/mp
    //--------------------------------------------------
    iStream.read(m_HP[ATTR_CURRENT]);
    iStream.read(m_HP[ATTR_MAX]);
    iStream.read(m_MP[ATTR_CURRENT]);
    iStream.read(m_MP[ATTR_MAX]);

    //--------------------------------------------------
    // read misc
    //--------------------------------------------------
    iStream.read(m_Fame);

    //--------------------------------------------------
    // read skills
    //--------------------------------------------------
    for (uint i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        iStream.read(m_DomainLevels[i]);
    }


    //--------------------------------------------------------------------------------
    // read outlook bitset
    //--------------------------------------------------------------------------------
    DWORD outlook;
    iStream.read(outlook);
    m_Outlook = bitset<SLAYER_BIT_MAX>(outlook);

    //--------------------------------------------------------------------------------
    // read colors
    //--------------------------------------------------------------------------------
    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        iStream.read(m_Colors[i]);

    iStream.read(m_AdvancementLevel);

    __END_CATCH
}

//----------------------------------------------------------------------
// write data to socket output stream
//----------------------------------------------------------------------
void PCSlayerInfo::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    //--------------------------------------------------
    // write slayer name
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
    // write attributes
    // Send only the pure attributes.
    //--------------------------------------------------
    oStream.write(m_STR);
    oStream.write(m_DEX);
    oStream.write(m_INT);

    //--------------------------------------------------
    // write attributes
    //--------------------------------------------------
    oStream.write(m_Rank);

    oStream.write(m_STRExp);
    oStream.write(m_DEXExp);
    oStream.write(m_INTExp);

    //--------------------------------------------------
    // write hp/mp
    //--------------------------------------------------
    oStream.write(m_HP[ATTR_CURRENT]);
    oStream.write(m_HP[ATTR_MAX]);
    oStream.write(m_MP[ATTR_CURRENT]);
    oStream.write(m_MP[ATTR_MAX]);

    //--------------------------------------------------
    // write misc
    //--------------------------------------------------
    oStream.write(m_Fame);

    //--------------------------------------------------
    // write skills
    //--------------------------------------------------
    for (uint i = 0; i < SKILL_DOMAIN_VAMPIRE; i++) {
        oStream.write(m_DomainLevels[i]);
    }


    //--------------------------------------------------------------------------------
    // write outlook bitset
    //--------------------------------------------------------------------------------
    DWORD outlook = m_Outlook.to_ulong();
    oStream.write(outlook);

    //--------------------------------------------------------------------------------
    // write colors
    //--------------------------------------------------------------------------------
    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        oStream.write(m_Colors[i]);

    oStream.write(m_AdvancementLevel);

    __END_CATCH
}

//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
string PCSlayerInfo::toString() const {
    StringStream msg;

    msg << "PCSlayerInfo(" << "NSize:" << m_Name.size() << ",Name:" << m_Name
        << ",Slot:" << nameOrNumber(Slot2String, m_Slot) << ",Alignment:" << m_Alignment << ",Rank:" << m_Rank
        << ",STR[BASIC]:" << (int)m_STR << ",DEX[BASIC]:" << (int)m_DEX << ",INT[BASIC]:" << (int)m_INT
        << ",HP:" << m_HP[ATTR_CURRENT] << "/" << m_HP[ATTR_MAX] << ",MP:" << m_MP[ATTR_CURRENT] << "/"
        << m_MP[ATTR_MAX] << ",Fame:" << m_Fame;
    //		<< ",Gold:" << m_Gold;

    for (uint i = 0; i < SKILL_DOMAIN_VAMPIRE; i++)
        msg << "," << nameOrNumber(SkillDomain2String, i) << ":" << (int)m_DomainLevels[i];

    msg << ",Sex:" << nameOrNumber(Sex2String, getSex())
        << ",HairStyle:" << nameOrNumber(HairStyle2String, getHairStyle()) << ",HairColor:" << (int)getHairColor()
        << ",SkinColor:" << (int)getSkinColor() << " ,Helmet:" << (int)getHelmetType()
        << ",HelmetColor:" << (int)getHelmetColor() << " ,Jacket:" << (int)getJacketType()
        << ",JacketColor:" << (int)getJacketColor() << " ,Pants:" << (int)getPantsType()
        << ",PantsColor:" << (int)getPantsColor() << " ,Weapon:" << (int)getWeaponType()
        << ",WeaponColor:" << (int)getWeaponColor() << " ,Shield:" << (int)getShieldType()
        << ",ShieldColor:" << (int)getShieldColor() << ")";

    return msg.toString();
}
