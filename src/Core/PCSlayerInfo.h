//////////////////////////////////////////////////////////////////////////////
// Filename    : PCSlayerInfo.h
// Written By  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __PC_SLAYER_INFO_H__
#define __PC_SLAYER_INFO_H__

#include <bitset>

#include "PCInfo.h"
#include "WireString.h"
#include "types/SlayerWeaponShape.h"

//////////////////////////////////////////////////////////////////////////////
// Object that carries Slayer information.
// It is carried in the GCPCList packet and sent to the client.
// It does not carry information such as items or the magic in effect.
//////////////////////////////////////////////////////////////////////////////

class PCSlayerInfo : public PCInfo {
public:
    // Slayer outlook, one DWORD on the wire. The weapon is two fields.
    // WEAPON1..4 hold the four-bit view: the weapon itself below 16, and
    // above that the stand-in slayerWeaponListShape gives it (a cross for
    // cross1, no weapon for mace and mace1). WEAPON_EXT1..2, past the
    // shield, hold an extension code: 0 when the four bits are the weapon,
    // otherwise the weapon's distance past WEAPON_CROSS (1 cross1, 2 mace,
    // 3 mace1). A client that knows only the four weapon bits keeps bits
    // 0-16 of the DWORD, so it reads the four-bit view and never the code.
    enum SlayerBits {
        SLAYER_BIT_SEX,
        SLAYER_BIT_HAIRSTYLE1,
        SLAYER_BIT_HAIRSTYLE2,
        SLAYER_BIT_HELMET1,
        SLAYER_BIT_HELMET2,
        SLAYER_BIT_JACKET1,
        SLAYER_BIT_JACKET2,
        SLAYER_BIT_JACKET3,
        SLAYER_BIT_PANTS1,
        SLAYER_BIT_PANTS2,
        SLAYER_BIT_PANTS3,
        SLAYER_BIT_WEAPON1,
        SLAYER_BIT_WEAPON2,
        SLAYER_BIT_WEAPON3,
        SLAYER_BIT_WEAPON4,
        SLAYER_BIT_SHIELD1,
        SLAYER_BIT_SHIELD2,
        SLAYER_BIT_WEAPON_EXT1,
        SLAYER_BIT_WEAPON_EXT2,
        SLAYER_BIT_MAX
    };

    // Both weapon fields, and nothing else.
    static constexpr DWORD kWeaponBitsMask = (15u << SLAYER_BIT_WEAPON1) | (3u << SLAYER_BIT_WEAPON_EXT1);

    // The outlook bits of a weapon: its four-bit view and its extension
    // code. setWeaponType and the gameserver's slayerListWeaponBits, which
    // Slayer::getShapeInfo writes Slayer.Shape with, both write through
    // this, so the two fields never disagree. A value past WEAPON_MACE1 is
    // no weapon: both fields zero.
    static constexpr DWORD weaponBits(DWORD weaponType) {
        if (weaponType >= WEAPON_MAX)
            return 0;
        const DWORD fourBitView = slayerWeaponListShape(WeaponType(weaponType));
        const DWORD extension = weaponType > WEAPON_CROSS ? weaponType - WEAPON_CROSS : 0;
        return (fourBitView << SLAYER_BIT_WEAPON1) | (extension << SLAYER_BIT_WEAPON_EXT1);
    }

    // The weapon an outlook names: the extension code when it is set, the
    // four bits otherwise. Every outlook decodes to 0..WEAPON_MACE1 (the
    // static_asserts below the class), so a table of WEAPON_MAX entries
    // can be indexed with the result unchecked; the last line keeps that
    // true if the enum ever changes under it.
    static constexpr WeaponType weaponFromBits(DWORD outlook) {
        const DWORD extension = (outlook >> SLAYER_BIT_WEAPON_EXT1) & 3;
        const DWORD weapon = extension != 0 ? WEAPON_CROSS + extension : (outlook >> SLAYER_BIT_WEAPON1) & 15;
        return weapon < WEAPON_MAX ? WeaponType(weapon) : WEAPON_NONE;
    }

    // Slayer Color Informations
    enum SlayerColors {
        SLAYER_COLOR_HAIR,
        SLAYER_COLOR_SKIN,
        SLAYER_COLOR_HELMET,
        SLAYER_COLOR_JACKET,
        SLAYER_COLOR_PANTS,
        SLAYER_COLOR_WEAPON,
        SLAYER_COLOR_SHIELD,
        SLAYER_COLOR_MAX
    };

public:
    // get pc type
    PCType getPCType() const {
        return PC_SLAYER;
    }

    // read data from socket input stream
    void read(SocketInputStream& iStream);

    // write data to socket output stream
    void write(SocketOutputStream& oStream) const;

    // get size of object
    uint getSize() const {
        return de::wire::stringWireSize(m_Name) + szSlot + szAlignment + szAttr * 3 + szRank + szExp * 3 + szHP * 2 +
               szMP * 2 +
               szFame
               //+ szGold
               + szSkillLevel * 6
               //+ szZoneID
               + szDWORD                    // Slayer flags
               + szColor * SLAYER_COLOR_MAX // Colour information
               + szLevel;
    }

    // get max size of object
    static constexpr uint getMaxSize() {
        return szBYTE + 20 + szSlot + szAlignment + szAttr * 3 + szRank + szExp * 3 + szHP * 2 + szMP * 2 +
               szFame
               //+ szGold
               + szSkillLevel * 6
               //+ szZoneID
               + szDWORD                    // Slayer flags
               + szColor * SLAYER_COLOR_MAX // Colour information
               + szLevel;
    }

    // get debug string
    string toString() const;

public:
    // get/set PC's name
    string getName() const {
        return m_Name;
    }
    void setName(const string& name) {
        m_Name = (name.size() > 20) ? name.substr(0, 20) : name;
    }

    // get/set Slot
    Slot getSlot() const {
        return m_Slot;
    }
    void setSlot(Slot slot) {
        m_Slot = slot;
    }
    void setSlot(string slot) {
        if (slot == Slot2String[SLOT1]) {
            m_Slot = SLOT1;
        } else if (slot == Slot2String[SLOT2]) {
            m_Slot = SLOT2;
        } else if (slot == Slot2String[SLOT3]) {
            m_Slot = SLOT3;
        } else {
            throw InvalidProtocolException("invalid slot value");
        }
    }

    // get/set Alignment
    Alignment_t getAlignment() const {
        return m_Alignment;
    }
    void setAlignment(Alignment_t Alignment) {
        m_Alignment = Alignment;
    }

    // get/set STR
    // *CAUTION*
    // With Assert() it would be disabled in NDEBUG mode, so check with if instead.
    Attr_t getSTR() const {
        if (m_STR > maxSlayerAttr)
            throw Error("STR out of range");
        return m_STR;
    }
    void setSTR(Attr_t str) {
        if (str > maxSlayerAttr)
            throw Error("STR out of range");
        m_STR = str;
    }

    // get/set DEX
    Attr_t getDEX() const {
        if (m_DEX > maxSlayerAttr)
            throw Error("DEX out of range");
        return m_DEX;
    }
    void setDEX(Attr_t dex) {
        if (dex > maxSlayerAttr)
            throw Error("DEX out of range");
        m_DEX = dex;
    }

    // get/set INT
    Attr_t getINT() const {
        if (m_INT > maxSlayerAttr)
            throw Error("INT out of range");
        return m_INT;
    }
    void setINT(Attr_t inte) {
        if (inte > maxSlayerAttr)
            throw Error("INT out of range");
        m_INT = inte;
    }

    // get/set STR Exp
    Exp_t getSTRExp() const {
        return m_STRExp;
    };
    void setSTRExp(Exp_t STRExp) {
        m_STRExp = STRExp;
    }

    // get/set DEX Exp
    Exp_t getDEXExp() const {
        return m_DEXExp;
    };
    void setDEXExp(Exp_t DEXExp) {
        m_DEXExp = DEXExp;
    }

    // get/set INT Exp
    Exp_t getINTExp() const {
        return m_INTExp;
    };
    void setINTExp(Exp_t INTExp) {
        m_INTExp = INTExp;
    }


    Rank_t getRank() const {
        return m_Rank;
    }
    void setRank(Rank_t rank) {
        m_Rank = rank;
    }


    // get/set HP
    HP_t getHP(AttrType attrType = ATTR_CURRENT) const {
        return m_HP[attrType];
    }
    void setHP(HP_t hp, AttrType attrType = ATTR_CURRENT) {
        m_HP[attrType] = hp;
    }
    void setHP(HP_t curHP, HP_t maxHP) {
        m_HP[ATTR_CURRENT] = curHP;
        m_HP[ATTR_MAX] = maxHP;
    }

    // get/set MP
    MP_t getMP(AttrType attrType = ATTR_CURRENT) const {
        return m_MP[attrType];
    }
    void setMP(MP_t mp, AttrType attrType = ATTR_CURRENT) {
        m_MP[attrType] = mp;
    }
    void setMP(MP_t curMP, MP_t maxMP) {
        m_MP[ATTR_CURRENT] = curMP;
        m_MP[ATTR_MAX] = maxMP;
    }

    // get/set fame
    Fame_t getFame() const {
        return m_Fame;
    }
    void setFame(Fame_t fame) {
        m_Fame = fame;
    }

    // get/set gold
    // Gold_t getGold () const  { return m_Gold; }
    // void setGold (Gold_t gold)  { m_Gold = gold; }

    // get/set skill domain level
    SkillLevel_t getSkillDomainLevel(SkillDomain domain) const {
        return m_DomainLevels[domain];
    }
    void setSkillDomainLevel(SkillDomain domain, SkillLevel_t skillLevel) {
        m_DomainLevels[domain] = skillLevel;
    }

    // get/set zoneID
    // ZoneID_t getZoneID () const  { return m_ZoneID; }
    // void setZoneID (ZoneID_t zoneID)  { m_ZoneID = zoneID; }

    Level_t getAdvancementLevel() const {
        return m_AdvancementLevel;
    }
    void setAdvancementLevel(Level_t level) {
        m_AdvancementLevel = level;
    }

    // get/set outlook
public:
    // get/set sex
    Sex getSex() const {
        return m_Outlook.test(SLAYER_BIT_SEX) ? MALE : FEMALE;
    }
    void setSex(Sex sex) {
        m_Outlook.set(SLAYER_BIT_SEX, (sex == MALE ? true : false));
    }

    void setSex(string sex) {
        if (sex == Sex2String[MALE])
            m_Outlook.set(SLAYER_BIT_SEX, true);
        else if (sex == Sex2String[FEMALE])
            m_Outlook.set(SLAYER_BIT_SEX, false);
        else
            throw InvalidProtocolException("invalid sex value");
    }

    // get/set hair style
    HairStyle getHairStyle() const {
        return HairStyle((m_Outlook.to_ulong() >> SLAYER_BIT_HAIRSTYLE1) & 3);
    }
    void setHairStyle(HairStyle hairStyle) {
        setOutlookField(SLAYER_BIT_HAIRSTYLE1, 3, hairStyle);
    }

    void setHairStyle(string hairStyle) {
        if (hairStyle == HairStyle2String[HAIR_STYLE1]) {
            m_Outlook &= ~bitset<SLAYER_BIT_MAX>(3 << SLAYER_BIT_HAIRSTYLE1);
            m_Outlook |= bitset<SLAYER_BIT_MAX>(HAIR_STYLE1 << SLAYER_BIT_HAIRSTYLE1);
        } else if (hairStyle == HairStyle2String[HAIR_STYLE2]) {
            m_Outlook &= ~bitset<SLAYER_BIT_MAX>(3 << SLAYER_BIT_HAIRSTYLE1);
            m_Outlook |= bitset<SLAYER_BIT_MAX>(HAIR_STYLE2 << SLAYER_BIT_HAIRSTYLE1);
        } else if (hairStyle == HairStyle2String[HAIR_STYLE3]) {
            m_Outlook &= ~bitset<SLAYER_BIT_MAX>(3 << SLAYER_BIT_HAIRSTYLE1);
            m_Outlook |= bitset<SLAYER_BIT_MAX>(HAIR_STYLE3 << SLAYER_BIT_HAIRSTYLE1);
        } else
            throw InvalidProtocolException("invalid hairstyle value");
    }

    // get/set helmet
    HelmetType getHelmetType() const {
        return HelmetType((m_Outlook.to_ulong() >> SLAYER_BIT_HELMET1) & 3);
    }
    void setHelmetType(HelmetType helmetType) {
        setOutlookField(SLAYER_BIT_HELMET1, 3, helmetType);
    }

    // get/set jacket
    JacketType getJacketType() const {
        return JacketType((m_Outlook.to_ulong() >> SLAYER_BIT_JACKET1) & 7);
    }
    void setJacketType(JacketType jacketType) {
        setOutlookField(SLAYER_BIT_JACKET1, 7, jacketType);
    }

    // get/set pants
    PantsType getPantsType() const {
        return PantsType((m_Outlook.to_ulong() >> SLAYER_BIT_PANTS1) & 7);
    }
    void setPantsType(PantsType pantsType) {
        setOutlookField(SLAYER_BIT_PANTS1, 7, pantsType);
    }

    // get/set weapon
    WeaponType getWeaponType() const {
        return weaponFromBits(m_Outlook.to_ulong());
    }
    void setWeaponType(WeaponType weaponType) {
        m_Outlook &= ~bitset<SLAYER_BIT_MAX>(kWeaponBitsMask);
        m_Outlook |= bitset<SLAYER_BIT_MAX>(weaponBits(weaponType));
    }

    // get/set Shield Type
    ShieldType getShieldType() const {
        return ShieldType((m_Outlook.to_ulong() >> SLAYER_BIT_SHIELD1) & 3);
    }
    void setShieldType(ShieldType shieldType) {
        setOutlookField(SLAYER_BIT_SHIELD1, 3, shieldType);
    }

    void setShapeInfo(DWORD flag, Color_t color[SLAYER_COLOR_MAX]);

    // get/set color
public:
    // get/set hair color
    Color_t getHairColor() const {
        return m_Colors[SLAYER_COLOR_HAIR];
    }
    void setHairColor(Color_t color) {
        m_Colors[SLAYER_COLOR_HAIR] = color;
    }

    // get/set skin color
    Color_t getSkinColor() const {
        return m_Colors[SLAYER_COLOR_SKIN];
    }
    void setSkinColor(Color_t color) {
        m_Colors[SLAYER_COLOR_SKIN] = color;
    }

    // get/set helmet color
    Color_t getHelmetColor(ColorType colorType = MAIN_COLOR) const {
        return m_Colors[SLAYER_COLOR_HELMET + (uint)colorType];
    }
    void setHelmetColor(Color_t color, ColorType colorType = MAIN_COLOR) {
        m_Colors[SLAYER_COLOR_HELMET + (uint)colorType] = color;
    }

    // get/set jacket color
    Color_t getJacketColor(ColorType colorType = MAIN_COLOR) const {
        return m_Colors[SLAYER_COLOR_JACKET + (uint)colorType];
    }
    void setJacketColor(Color_t color, ColorType colorType = MAIN_COLOR) {
        m_Colors[SLAYER_COLOR_JACKET + (uint)colorType] = color;
    }

    // get/set pants color
    Color_t getPantsColor(ColorType colorType = MAIN_COLOR) const {
        return m_Colors[SLAYER_COLOR_PANTS + (uint)colorType];
    }
    void setPantsColor(Color_t color, ColorType colorType = MAIN_COLOR) {
        m_Colors[SLAYER_COLOR_PANTS + (uint)colorType] = color;
    }

    // get/set weapon color
    Color_t getWeaponColor(ColorType colorType = MAIN_COLOR) const {
        return m_Colors[SLAYER_COLOR_WEAPON + (uint)colorType];
    }
    void setWeaponColor(Color_t color, ColorType colorType = MAIN_COLOR) {
        m_Colors[SLAYER_COLOR_WEAPON + (uint)colorType] = color;
    }

    // get/set shield color
    Color_t getShieldColor(ColorType colorType = MAIN_COLOR) const {
        return m_Colors[SLAYER_COLOR_SHIELD + (uint)colorType];
    }
    void setShieldColor(Color_t color, ColorType colorType = MAIN_COLOR) {
        m_Colors[SLAYER_COLOR_SHIELD + (uint)colorType] = color;
    }

private:
    // PC name
    string m_Name;

    // PC slot
    Slot m_Slot;

    // Alignment
    Alignment_t m_Alignment;

    // *NOTE
    // ATTR_BASIC   : the pure attribute.
    Attr_t m_STR;
    Attr_t m_DEX;
    Attr_t m_INT;

    // Current experience towards raising the attribute
    // The goal experience for the next level and the total experience are
    // held in an Exp Table on the client too, so
    // let the client work them out.
    Exp_t m_STRExp;
    Exp_t m_DEXExp;
    Exp_t m_INTExp;

    // Rank
    Rank_t m_Rank;

    // HP/MP
    // HP[0] = current hp, hp[1] == max hp
    HP_t m_HP[2];
    MP_t m_MP[2];

    // Fame
    Fame_t m_Fame;

    // skill domain levels
    SkillLevel_t m_DomainLevels[6];


    // Replaces the outlook field `mask` wide at bit `first` with the low
    // bits of value. A value wider than its field is cut to the field, so
    // no setter writes into a neighbour.
    void setOutlookField(SlayerBits first, DWORD mask, DWORD value) {
        m_Outlook &= ~bitset<SLAYER_BIT_MAX>(mask << first);
        m_Outlook |= bitset<SLAYER_BIT_MAX>((value & mask) << first);
    }

    bitset<SLAYER_BIT_MAX> m_Outlook;   // Slayer appearance information
    Color_t m_Colors[SLAYER_COLOR_MAX]; // Slayer colour information

    Level_t m_AdvancementLevel;
};

// The extension code counts from WEAPON_CROSS, so its largest value (3)
// decodes to WEAPON_MACE1 and no outlook decodes past the enum.
static_assert(WEAPON_CROSS == 15 && WEAPON_CROSS1 == 16 && WEAPON_MACE == 17 && WEAPON_MACE1 == 18 && WEAPON_MAX == 19,
              "the slayer outlook's weapon extension code names WEAPON_CROSS + 1..3");
static_assert(PCSlayerInfo::SLAYER_BIT_SHIELD2 == 16 && PCSlayerInfo::SLAYER_BIT_WEAPON_EXT1 == 17 &&
                  PCSlayerInfo::SLAYER_BIT_MAX == 19,
              "the weapon extension code sits past the shield, at bits 17-18");
static_assert(
    [] {
        for (DWORD weapon = 0; weapon < WEAPON_MAX; weapon++)
            if (DWORD(PCSlayerInfo::weaponFromBits(PCSlayerInfo::weaponBits(weapon))) != weapon)
                return false;
        return PCSlayerInfo::weaponBits(WEAPON_MAX) == 0;
    }(),
    "every weapon reads back as itself, and a value past WEAPON_MACE1 writes no weapon");
// Every real value of an outlook field's type, the enumerators below its
// _MAX, fits the field, so the setters' cut never changes one.
static_assert(HAIR_STYLE3 <= 3 && HELMET_MAX - 1 <= 3 && JACKET_MAX - 1 <= 7 && PANTS_MAX - 1 <= 7 &&
                  SHIELD_MAX - 1 <= 3,
              "each slayer outlook field holds every value of its type below its _MAX");

#endif
