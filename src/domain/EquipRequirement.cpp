//////////////////////////////////////////////////////////////////////////////
// Filename    : EquipRequirement.cpp
// Description :
// An item's equip requirement and whether a wearer meets it. See
// EquipRequirement.h.
//////////////////////////////////////////////////////////////////////////////

#include "domain/EquipRequirement.h"

#include <algorithm>

namespace decore {

namespace {

// The server's Attr_t and Level_t.
typedef unsigned short Attr;
typedef unsigned char Level;

const Attr MAX_SLAYER_ATTR = 290;
const Attr MAX_SLAYER_SUM = 435;
const Attr MAX_SLAYER_ATTR_OLD = 200;
const Attr MAX_SLAYER_SUM_OLD = 300;

const Level MAX_VAMPIRE_LEVEL = 150;
const Level MAX_VAMPIRE_LEVEL_OLD = 100;

const Level MAX_OUSTERS_LEVEL = 150;

// The sums are taken in unsigned arithmetic, which wraps where the
// server's int arithmetic would overflow and otherwise keeps the same low
// bits, so the narrowed result is the server's.
Attr addAttr(Attr value, unsigned increase) {
    return (Attr)(value + increase);
}

Level addLevel(Level value, unsigned increase) {
    return (Level)(value + increase);
}

EquipRequirement slayerRequirement(const EquipRequirement& base, const int* optionReqSums, int count) {
    Attr ReqSTR = (Attr)base.str;
    Attr ReqDEX = (Attr)base.dex;
    Attr ReqINT = (Attr)base.inte;
    Attr ReqSum = (Attr)base.sum;

    // An item table's requirement above the old cap may be raised by its
    // options up to the new cap; one at or below it stays within the old.
    Attr ReqSumMax = ((ReqSum > MAX_SLAYER_SUM_OLD) ? MAX_SLAYER_SUM : MAX_SLAYER_SUM_OLD);
    Attr ReqSTRMax = ((ReqSTR > MAX_SLAYER_ATTR_OLD) ? MAX_SLAYER_ATTR : MAX_SLAYER_ATTR_OLD);
    Attr ReqDEXMax = ((ReqDEX > MAX_SLAYER_ATTR_OLD) ? MAX_SLAYER_ATTR : MAX_SLAYER_ATTR_OLD);
    Attr ReqINTMax = ((ReqINT > MAX_SLAYER_ATTR_OLD) ? MAX_SLAYER_ATTR : MAX_SLAYER_ATTR_OLD);

    for (int i = 0; i < count; i++) {
        const unsigned optionSum = (unsigned)optionReqSums[i];
        if (ReqSTR != 0)
            ReqSTR = addAttr(ReqSTR, optionSum * 2u);
        if (ReqDEX != 0)
            ReqDEX = addAttr(ReqDEX, optionSum * 2u);
        if (ReqINT != 0)
            ReqINT = addAttr(ReqINT, optionSum * 2u);
        if (ReqSum != 0)
            ReqSum = addAttr(ReqSum, optionSum);
    }

    EquipRequirement required = base;
    required.str = std::min(ReqSTR, ReqSTRMax);
    required.dex = std::min(ReqDEX, ReqDEXMax);
    required.inte = std::min(ReqINT, ReqINTMax);
    required.sum = std::min(ReqSum, ReqSumMax);
    required.gender = (Attr)base.gender;
    return required;
}

EquipRequirement vampireRequirement(const EquipRequirement& base, const int* optionReqLevels, int count) {
    Level ReqLevel = (Level)base.level;

    // A table level above the old cap may be raised by the options up to
    // the new cap; one at or below it stays within the old.
    Level ReqLevelMax = ((ReqLevel > MAX_VAMPIRE_LEVEL_OLD) ? MAX_VAMPIRE_LEVEL : MAX_VAMPIRE_LEVEL_OLD);

    for (int i = 0; i < count; i++)
        ReqLevel = addLevel(ReqLevel, (unsigned)optionReqLevels[i]);

    EquipRequirement required = base;
    required.level = std::min(ReqLevel, ReqLevelMax);
    required.gender = (Attr)base.gender;
    return required;
}

EquipRequirement oustersRequirement(const EquipRequirement& base, const int* optionReqSums, const int* optionReqLevels,
                                    int count) {
    Level ReqLevel = (Level)base.level;
    Attr ReqSTR = (Attr)base.str;
    Attr ReqDEX = (Attr)base.dex;
    Attr ReqINT = (Attr)base.inte;
    Attr ReqSum = (Attr)base.sum;

    for (int i = 0; i < count; i++) {
        const unsigned optionSum = (unsigned)optionReqSums[i];
        if (ReqLevel != 0)
            ReqLevel = addLevel(ReqLevel, (unsigned)optionReqLevels[i]);
        if (ReqSTR != 0)
            ReqSTR = addAttr(ReqSTR, optionSum * 2u);
        if (ReqDEX != 0)
            ReqDEX = addAttr(ReqDEX, optionSum * 2u);
        if (ReqINT != 0)
            ReqINT = addAttr(ReqINT, optionSum * 2u);
        if (ReqSum != 0)
            ReqSum = addAttr(ReqSum, optionSum);
    }

    EquipRequirement required = base;
    required.level = std::min(ReqLevel, MAX_OUSTERS_LEVEL);
    required.str = ReqSTR;
    required.dex = ReqDEX;
    required.inte = ReqINT;
    required.sum = ReqSum;
    return required;
}

} // namespace

EquipRequirement requiredStats(EquipRace race, const EquipRequirement& base, const int* optionReqSums,
                               const int* optionReqLevels, int count) {
    switch (race) {
    case EquipRace::Slayer:
        return slayerRequirement(base, optionReqSums, count);
    case EquipRace::Vampire:
        return vampireRequirement(base, optionReqLevels, count);
    case EquipRace::Ousters:
        return oustersRequirement(base, optionReqSums, optionReqLevels, count);
    }
    return base;
}

bool genderAllows(int sex, int reqGender) {
    const Attr ReqGender = (Attr)reqGender;
    return !((sex == sex::Male && ReqGender == gender::Female) || (sex == sex::Female && ReqGender == gender::Male));
}

bool meetsRequirement(EquipRace race, const EquipRequirement& required, const EquipStats& current) {
    const Attr ReqSTR = (Attr)required.str;
    const Attr ReqDEX = (Attr)required.dex;
    const Attr ReqINT = (Attr)required.inte;
    const Attr ReqSum = (Attr)required.sum;
    const Level ReqLevel = (Level)required.level;

    const Attr CSTR = (Attr)current.str;
    const Attr CDEX = (Attr)current.dex;
    const Attr CINT = (Attr)current.inte;
    const Attr CSUM = (Attr)(CSTR + CDEX + CINT);
    const Level CLevel = (Level)current.level;

    switch (race) {
    case EquipRace::Slayer:
        return !(CSTR < ReqSTR || CDEX < ReqDEX || CINT < ReqINT || CSUM < ReqSum ||
                 !genderAllows(current.sex, required.gender));
    case EquipRace::Vampire:
        if (ReqLevel > 0 && CLevel < ReqLevel)
            return false;
        return genderAllows(current.sex, required.gender);
    case EquipRace::Ousters:
        return !(CSTR < ReqSTR || CDEX < ReqDEX || CINT < ReqINT || CSUM < ReqSum || CLevel < ReqLevel);
    }
    return false;
}

} // namespace decore
