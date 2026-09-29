//////////////////////////////////////////////////////////////////////////////
// Filename    : PCOustersInfo2.cpp
// Written By  : Reiot
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "PCOustersInfo2.h"

#include "WireString.h"

//////////////////////////////////////////////////////////////////////////////
// read data from socket input stream
//////////////////////////////////////////////////////////////////////////////
void PCOustersInfo2::read(SocketInputStream& iStream) {
    __BEGIN_TRY

    //--------------------------------------------------
    // read object id
    //--------------------------------------------------
    iStream.read(m_ObjectID);

    //--------------------------------------------------
    // read vampire name
    //--------------------------------------------------

    de::wire::readString(iStream, m_Name, {1, 20}, "Name");

    //--------------------------------------------------
    // read Level
    //--------------------------------------------------
    iStream.read(m_Level);

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
    iStream.read(m_HairColor);
    iStream.read(m_MasterEffectColor);

    //--------------------------------------------------
    // read Alignment
    //--------------------------------------------------
    iStream.read(m_Alignment);
    //--------------------------------------------------
    // read attributes
    //--------------------------------------------------
    iStream.read(m_STR[ATTR_CURRENT]);
    iStream.read(m_STR[ATTR_MAX]);
    iStream.read(m_STR[ATTR_BASIC]);
    iStream.read(m_DEX[ATTR_CURRENT]);
    iStream.read(m_DEX[ATTR_MAX]);
    iStream.read(m_DEX[ATTR_BASIC]);
    iStream.read(m_INT[ATTR_CURRENT]);
    iStream.read(m_INT[ATTR_MAX]);
    iStream.read(m_INT[ATTR_BASIC]);

    //--------------------------------------------------
    // read hp
    //--------------------------------------------------
    iStream.read(m_HP[ATTR_CURRENT]);
    iStream.read(m_HP[ATTR_MAX]);

    //--------------------------------------------------
    // read mp
    //--------------------------------------------------
    iStream.read(m_MP[ATTR_CURRENT]);
    iStream.read(m_MP[ATTR_MAX]);

    //--------------------------------------------------
    // read misc
    //--------------------------------------------------
    iStream.read(m_Rank);
    iStream.read(m_RankExp);

    iStream.read(m_Exp);
    iStream.read(m_Gold);


    //--------------------------------------------------
    // read fame&notoriety
    //--------------------------------------------------
    iStream.read(m_Fame);

    //--------------------------------------------------
    // read sight
    //--------------------------------------------------
    iStream.read(m_Sight);

    //--------------------------------------------------
    // read bonus
    //--------------------------------------------------
    iStream.read(m_Bonus);
    iStream.read(m_SkillBonus);

    // Silver coating damage
    iStream.read(m_SilverDamage);

    // Competence
    iStream.read(m_Competence);

    // Guild ID
    iStream.read(m_GuildID);

    de::wire::readString(iStream, m_GuildName, {0, 30}, "GuildName");

    iStream.read(m_GuildMemberRank);
    iStream.read(m_UnionID);
    iStream.read(m_AdvancementLevel);
    iStream.read(m_AdvancementGoalExp);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// write data to socket output stream
//////////////////////////////////////////////////////////////////////////////
void PCOustersInfo2::write(SocketOutputStream& oStream) const {
    __BEGIN_TRY

    //--------------------------------------------------
    // write object id
    //--------------------------------------------------
    oStream.write(m_ObjectID);

    //--------------------------------------------------
    // write vampire name
    //--------------------------------------------------
    de::wire::writeString(oStream, m_Name, {1, 20}, "Name");

    //--------------------------------------------------
    // write sex
    //--------------------------------------------------
    oStream.write(m_Level);

    //--------------------------------------------------
    // write sex
    //--------------------------------------------------
    oStream.write((BYTE)m_Sex);

    //--------------------------------------------------
    // write colors
    //--------------------------------------------------
    oStream.write(m_HairColor);
    oStream.write(m_MasterEffectColor);

    //--------------------------------------------------
    // write Alignment
    //--------------------------------------------------
    oStream.write(m_Alignment);

    //--------------------------------------------------
    // write attributes
    //--------------------------------------------------
    oStream.write(m_STR[ATTR_CURRENT]);
    oStream.write(m_STR[ATTR_MAX]);
    oStream.write(m_STR[ATTR_BASIC]);
    oStream.write(m_DEX[ATTR_CURRENT]);
    oStream.write(m_DEX[ATTR_MAX]);
    oStream.write(m_DEX[ATTR_BASIC]);
    oStream.write(m_INT[ATTR_CURRENT]);
    oStream.write(m_INT[ATTR_MAX]);
    oStream.write(m_INT[ATTR_BASIC]);

    //--------------------------------------------------
    // write hp
    //--------------------------------------------------
    oStream.write(m_HP[ATTR_CURRENT]);
    oStream.write(m_HP[ATTR_MAX]);

    //--------------------------------------------------
    // write mp
    //--------------------------------------------------
    oStream.write(m_MP[ATTR_CURRENT]);
    oStream.write(m_MP[ATTR_MAX]);

    //--------------------------------------------------
    // write misc
    //--------------------------------------------------
    oStream.write(m_Rank);
    oStream.write(m_RankExp);

    oStream.write(m_Exp);
    oStream.write(m_Gold);

    //--------------------------------------------------
    // read fame&notoriety
    //--------------------------------------------------
    oStream.write(m_Fame);

    //--------------------------------------------------
    // write sight
    //--------------------------------------------------
    oStream.write(m_Sight);

    //--------------------------------------------------
    // write Bonus
    //--------------------------------------------------
    oStream.write(m_Bonus);
    oStream.write(m_SkillBonus);

    // Silver coating damage
    oStream.write(m_SilverDamage);

    // Competence
    oStream.write(m_Competence);

    // Guild ID
    oStream.write(m_GuildID);

    de::wire::writeString(oStream, m_GuildName, {0, 30}, "GuildName");

    oStream.write(m_GuildMemberRank);
    oStream.write(m_UnionID);
    oStream.write(m_AdvancementLevel);
    oStream.write(m_AdvancementGoalExp);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string PCOustersInfo2::toString() const {
    StringStream msg;

    msg << "PCOustersInfo2(" << "ObjectID:" << m_ObjectID << ",Name:" << m_Name << ",Level:" << m_Level
        << ",Sex:" << nameOrNumber(Sex2String, m_Sex) << ",HairColor:" << (int)m_HairColor
        << ",MasterEffectColor:" << (int)m_MasterEffectColor << ",Alignment:" << (int)m_Alignment
        << ",STR[CURRENT]:" << (int)m_STR[ATTR_CURRENT] << ",STR[MAX]:" << (int)m_STR[ATTR_MAX]
        << ",STR[BASIC]:" << (int)m_STR[ATTR_BASIC] << ",DEX[CURRENT]:" << (int)m_DEX[ATTR_CURRENT]
        << ",DEX[MAX]:" << (int)m_DEX[ATTR_MAX] << ",DEX[BASIC]:" << (int)m_DEX[ATTR_BASIC]
        << ",INT[CURRENT]:" << (int)m_INT[ATTR_CURRENT] << ",INT[MAX]:" << (int)m_INT[ATTR_MAX]
        << ",INT[BASIC]:" << (int)m_INT[ATTR_BASIC] << ",HP:" << m_HP[ATTR_CURRENT] << "/" << m_HP[ATTR_MAX]
        << ",MP:" << m_MP[ATTR_CURRENT] << "/" << m_MP[ATTR_MAX] << ",Rank:" << (int)m_Rank
        << ",RankExp:" << (int)m_RankExp << ",Exp:" << m_Exp << ",Gold:" << m_Gold << ",Fame:" << m_Fame
        << ",Sight:" << (int)m_Sight << ",Bonus:" << (int)m_Bonus << ",SkillBonus:" << (int)m_SkillBonus
        << ",SilverDamage:" << (int)m_SilverDamage << ",Competence:" << (int)m_Competence
        << ",GuildID:" << (int)m_GuildID << ",GuildName:" << m_GuildName
        << ",GuildMemberRank:" << (int)m_GuildMemberRank << ")";

    return msg.toString();
}
