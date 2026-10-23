////////////////////////////////////////////////////////////////////////////// // Filename    : ItemInfo.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ItemInfo.h"

#include "GameContext.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "VolumeInfo.h"

//////////////////////////////////////////////////////////////////////////////
// class ItemInfo member methods
//////////////////////////////////////////////////////////////////////////////

ItemInfo::ItemInfo()

{
    m_ItemType = 0;
    m_Name = "";
    m_EName = "";
    m_Price = 0;
    m_VolumeType = 0;
    m_Weight = 0;
    m_Ratio = 0;
    m_Description = "";
    m_ReqSTR = 0;
    m_ReqDEX = 0;
    m_ReqINT = 0;
    m_ReqSum = 0;
    m_ReqLevel = 0;
    m_ReqAdvanceLevel = 0;
    m_ReqGender = GENDER_BOTH;

    m_bDefaultOptions = false;
    m_UpgradeRatio = 0;
    m_UpgradeCrashPercent = 0;

    m_bUnique = false;
    m_NextOptionRatio = 0;

    m_NextItemType = 0;
    m_DowngradeRatio = 0;
}

ItemInfo::~ItemInfo()

{}

VolumeWidth_t ItemInfo::getVolumeWidth() const

{
    return de::gameContext().volumeInfos().getVolumeInfo(m_VolumeType)->getVolumeWidth();
}

VolumeHeight_t ItemInfo::getVolumeHeight() const

{
    return de::gameContext().volumeInfos().getVolumeInfo(m_VolumeType)->getVolumeHeight();
}

void ItemInfo::setReqAbility(const string& req)

{
    size_t a = 0, b = 0, c = 0;

    do {
        //////////////////////////////////////////////
        // (DEX,40) (INT,40)
        // a   b  c a
        //////////////////////////////////////////////
        a = req.find_first_of('(', c);
        b = req.find_first_of(',', a + 1);
        c = req.find_first_of(')', b + 1);

        if (a > b || b > c)
            break;

        Assert(a + 1 < b && b + 1 < c);

        string identifier = req.substr(a + 1, b - a - 1);
        Attr_t reqAbility = (Attr_t)atoi(req.substr(b + 1, c - b - 1).c_str());

        if (identifier == "STR") {
            setReqSTR(reqAbility);
        } else if (identifier == "DEX") {
            setReqDEX(reqAbility);
        } else if (identifier == "INT") {
            setReqINT(reqAbility);
        } else if (identifier == "LEV") {
            setReqLevel(reqAbility);
        } else if (identifier == "GEN") {
            setReqGender(reqAbility);
        } else if (identifier == "SUM") {
            setReqSum(reqAbility);
        } else if (identifier == "ADV") {
            setReqAdvanceLevel(reqAbility);
        }

    } while (c < req.size() - 1);
}

void ItemInfo::setDefaultOptions(const string& options)

{
    //////////////////////////////////////////////
    // DEX+1,INT+2
    // a     ba     b
    //////////////////////////////////////////////
    if (options.size() <= 1) {
        clearDefaultOptions();
        return;
    }

    makeOptionList(options, m_DefaultOptions);
}


void ItemInfo::addDefaultOptions(OptionType_t optionType)

{
    m_bDefaultOptions = true;
    m_DefaultOptions.push_back(optionType);
}

void ItemInfo::clearDefaultOptions()

{
    m_bDefaultOptions = false;
    m_DefaultOptions.clear();
}

// Did the upgrade to the next option level succeed?
bool ItemInfo::isUpgradeSucceed() const {
    // It succeeds when the ratio is not 0 and rand() over that ratio
    // lands on one particular value (0).
    // return m_UpgradeRatio!=0 && (rand()%m_UpgradeRatio==0);

    // The design came out as a percentage instead.
    return (rand() % 100 < (int)m_UpgradeRatio);
}

// Did the upgrade to the next option level fail and break the item?
bool ItemInfo::isUpgradeCrash() const {
    return (rand() % 100 < m_UpgradeCrashPercent);
}
