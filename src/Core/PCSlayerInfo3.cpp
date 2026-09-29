//////////////////////////////////////////////////////////////////////////////
// Filename    : PCSlayerInfo3.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PCSlayerInfo3.h"

#include "WireString.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void PCSlayerInfo3::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    // read OID
    iStream.read(m_ObjectID);

    // read name
    de::wire::readString(iStream, m_Name, {1, 20}, "Name");

    // read (X,Y,Dir)
    iStream.read(m_X);
    iStream.read(m_Y);
    iStream.read(m_Dir);

    // read outlook bitset
    DWORD outlook;
    iStream.read(outlook);
    m_Outlook = bitset<SLAYER_BIT_MAX>(outlook);

    // read colors
    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        iStream.read(m_Colors[i]);

    iStream.read(m_MasterEffectColor);
    iStream.read(m_CurrentHP);
    iStream.read(m_MaxHP);
    iStream.read(m_AttackSpeed);
    iStream.read(m_Alignment);

    iStream.read(m_Competence);
    iStream.read(m_GuildID);
    iStream.read(m_UnionID);

    iStream.read(m_Rank);
    iStream.read(m_AdvancementLevel);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Send the packet's binary image to the output stream (buffer).
//////////////////////////////////////////////////////////////////////////////
void PCSlayerInfo3::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    // write OID
    oStream.write(m_ObjectID);

    // write name
    de::wire::writeString(oStream, m_Name, {1, 20}, "Name");

    // write (X,Y,Dir)
    oStream.write(m_X);
    oStream.write(m_Y);
    oStream.write(m_Dir);

    // write outlook bitset
    DWORD outlook = m_Outlook.to_ulong();
    oStream.write(outlook);

    // write colors
    for (uint i = 0; i < SLAYER_COLOR_MAX; i++)
        oStream.write(m_Colors[i]);

    oStream.write(m_MasterEffectColor);
    oStream.write(m_CurrentHP);
    oStream.write(m_MaxHP);

    oStream.write(m_AttackSpeed);

    oStream.write(m_Alignment);

    oStream.write(m_Competence);
    oStream.write(m_GuildID);
    oStream.write(m_UnionID);

    oStream.write(m_Rank);
    oStream.write(m_AdvancementLevel);


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////////////
string PCSlayerInfo3::toString() const {
    StringStream msg;

    msg << "PCSlayerInfo3(" << "ObjectID:" << m_ObjectID << ",Name:" << m_Name << ",X:" << (int)m_X << ",Y:" << (int)m_Y
        << ",Dir:" << dir2String(m_Dir) << ",Sex:" << sex2String(getSex())
        << ",HairStyle:" << nameOrNumber(HairStyle2String, getHairStyle()) << ",HairColor:" << (int)getHairColor()
        << ",SkinColor:" << (int)getSkinColor() << ",Helmet:" << nameOrNumber(HelmetType2String, getHelmetType());

    if (getHelmetType() != HELMET_NONE)
        msg << ",HelmetColor:" << (int)getHelmetColor();

    msg << ",Jacket:" << nameOrNumber(JacketType2String, getJacketType())
        << ",JacketColor:" << (int)getJacketColor(MAIN_COLOR) << "/" << (int)getJacketColor(SUB_COLOR)
        << ",Pants:" << nameOrNumber(PantsType2String, getPantsType())
        << ",PantsColor:" << (int)getPantsColor(MAIN_COLOR) << "/" << (int)getPantsColor(SUB_COLOR)
        << ",Weapon:" << nameOrNumber(WeaponType2String, getWeaponType()) << ",WeaponColor:" << (int)getWeaponColor()
        << ",Shield:" << nameOrNumber(ShieldType2String, getShieldType()) << ",ShieldColor:" << (int)getShieldColor()
        << ",Motorcycle:" << nameOrNumber(MotorcycleType2String, getMotorcycleType())
        << ",Shoulder:" << (int)getShoulderType();

    if (getMotorcycleType() != MOTORCYCLE_NONE)
        msg << ",MotorcycleColor:" << (int)getMotorcycleColor();

    msg << ",CurerntHP:" << (int)m_CurrentHP;
    msg << ",MaxHP:" << (int)m_MaxHP;
    msg << ",Alignment:" << (int)m_Alignment;
    msg << ",AttackSpeed:" << (int)m_AttackSpeed;
    msg << ",Competence:" << (int)m_Competence;
    msg << ",GuildID:" << (int)m_GuildID;
    msg << ",Rank:" << (int)m_Rank;
    msg << ")";

    return msg.toString();
}
