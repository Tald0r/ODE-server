//////////////////////////////////////////////////////////////////////////////
// Filename    : OptionInfo.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "OptionInfo.h"

#include "Assert.h"
#include "StringStream.h"
#include "repository/GameInfoRepository.h"
// #include <algo.h>

#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"

// Can an optionType be attached to this itemClass?
bool isPossibleOptionClass(Item::ItemClass itemClass, OptionClass optionClass) {
    switch (itemClass) {
    // WEAPON
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        switch (optionClass) {
        case OPTION_STR:
        case OPTION_DEX:
        case OPTION_INT:
        case OPTION_HP:
        case OPTION_MP:
        case OPTION_HP_STEAL:
        case OPTION_MP_STEAL:
        case OPTION_TOHIT:
        case OPTION_DAMAGE:
        case OPTION_DURABILITY:
        case OPTION_ATTACK_SPEED:
        case OPTION_LUCK:
        case OPTION_ALL_ATTR:
            return true;

        default:
            return false;
        }
        break;

    // ARMOR
    case Item::ITEM_CLASS_HELM:
    case Item::ITEM_CLASS_COAT:
    case Item::ITEM_CLASS_TROUSER:
    case Item::ITEM_CLASS_SHOES:
    case Item::ITEM_CLASS_GLOVE:
    case Item::ITEM_CLASS_BELT:
    case Item::ITEM_CLASS_SHIELD:
    case Item::ITEM_CLASS_VAMPIRE_COAT:
    case Item::ITEM_CLASS_OUSTERS_COAT:
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
    case Item::ITEM_CLASS_OUSTERS_CIRCLET:
    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
        switch (optionClass) {
        case OPTION_STR:
        case OPTION_DEX:
        case OPTION_INT:
        case OPTION_HP:
        case OPTION_MP:
        case OPTION_DEFENSE:
        case OPTION_PROTECTION:
        case OPTION_DURABILITY:
        case OPTION_LUCK:
        case OPTION_ALL_ATTR:
            return true;
            break;

        default:
            return false;
        }
        break;

    // ACCESSORY
    case Item::ITEM_CLASS_RING:
    case Item::ITEM_CLASS_BRACELET:
    case Item::ITEM_CLASS_NECKLACE:
    case Item::ITEM_CLASS_VAMPIRE_RING:
    case Item::ITEM_CLASS_VAMPIRE_EARRING:
    case Item::ITEM_CLASS_VAMPIRE_BRACELET:
    case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
    case Item::ITEM_CLASS_OUSTERS_PENDENT:
    case Item::ITEM_CLASS_OUSTERS_RING:
        switch (optionClass) {
        case OPTION_STR:
        case OPTION_DEX:
        case OPTION_INT:
        case OPTION_HP:
        case OPTION_MP:
        case OPTION_HP_STEAL:
        case OPTION_MP_STEAL:
        case OPTION_HP_REGEN:
        case OPTION_MP_REGEN:
        case OPTION_TOHIT:
        case OPTION_DAMAGE:
        case OPTION_DEFENSE:
        case OPTION_PROTECTION:
        case OPTION_DURABILITY:
        case OPTION_POISON:
        case OPTION_ACID:
        case OPTION_CURSE:
        case OPTION_BLOOD:
        case OPTION_VISION:
        case OPTION_ATTACK_SPEED:
        case OPTION_LUCK:
        case OPTION_ALL_RES:
        case OPTION_ALL_ATTR:
            return true;
            break;

        default:
            return false;
        }
        break;

    case Item::ITEM_CLASS_VAMPIRE_AMULET:
        switch (optionClass) {
        case OPTION_STR:
        case OPTION_DEX:
        case OPTION_INT:
        case OPTION_HP:
        case OPTION_MP:
        case OPTION_HP_STEAL:
        case OPTION_MP_STEAL:
        case OPTION_HP_REGEN:
        case OPTION_MP_REGEN:
        case OPTION_TOHIT:
        case OPTION_DAMAGE:
        case OPTION_DURABILITY:
        case OPTION_POISON:
        case OPTION_ACID:
        case OPTION_CURSE:
        case OPTION_BLOOD:
        case OPTION_VISION:
        case OPTION_ATTACK_SPEED:
        case OPTION_LUCK:
        case OPTION_ALL_RES:
        case OPTION_ALL_ATTR:
            return true;
            break;

        default:
            return false;
        }
        break;

    default:
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// class OptionInfo member methods
//////////////////////////////////////////////////////////////////////////////

OptionInfo::OptionInfo()

{
    m_OptionType = 0;
    m_Name = "";
    m_HName = "";
    m_Nickname = "";
    m_OptionClass = OPTION_MAX;
    m_PlusPoint = 0;
    m_PriceMultiplier = 0;
    m_ReqSTR = 0;
    m_ReqDEX = 0;
    m_ReqINT = 0;
    m_ReqSum = 0;
    m_ReqLevel = 0;
    m_Level = 0;
    m_Color = 0;
    m_Ratio = 0;
    m_GambleLevel = 0;

    // Information needed to upgrade to the next option stage.
    m_PreviousOptionType = 0;
    m_UpgradeOptionType = 0;
    m_UpgradeRatio = 0;
    m_UpgradeSecondRatio = 0;
    m_UpgradeCrashPercent = 0;
    m_NextOptionRatio = 0;
}

OptionInfo::~OptionInfo()

{
    __BEGIN_TRY
    __END_CATCH_NO_RETHROW
}

void OptionInfo::setReqAbility(const string& req)

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
        } else if (identifier == "SUM") {
            setReqSum(reqAbility);
        } else if (identifier != "") {
            // cerr << "OptionInfo::setReqAbility() : Unknown ability type [" << identifier << "]" << endl;
            throw Error("OptionInfo::setReqAbility() : Unknown ability type");
        }

    } while (c < req.size() - 1);
}

// Did the upgrade to the next option stage succeed?
bool OptionInfo::isUpgradeSucceed() const {
    // Not zero, and
    // success is rand() over the ratio landing on one particular value (0).
    // return m_UpgradeRatio!=0 && (rand()%m_UpgradeRatio==0);

    // The design came back in percentages instead.
    return (rand() % 100 < (int)m_UpgradeRatio);
}

// Did the Blue Drop 2 upgrade to the next option stage succeed?
bool OptionInfo::isUpgradeSecondSucceed() const {
    // Not zero, and
    // success is rand() over the ratio landing on one particular value (0).
    // return m_UpgradeRatio!=0 && (rand()%m_UpgradeRatio==0);

    // The design came back in percentages instead.
    return (rand() % 100 < (int)m_UpgradeSecondRatio);
}

// Did the item break from a failed upgrade to the next option stage?
bool OptionInfo::isUpgradeCrash() const {
    return (rand() % 100 < m_UpgradeCrashPercent);
}

string OptionInfo::toString() const

{
    StringStream msg;
    msg << "OptionInfo (" << "Type:" << (int)m_OptionType << "Name:" << m_Name << "HName:" << m_HName
        << "Nickname:" << m_Nickname << "Class:" << (int)m_OptionClass << "PlusPoint:" << (int)m_PlusPoint
        << "PriceMultiplier:" << (int)m_PriceMultiplier << "ReqSTR:" << (int)m_ReqSTR << "ReqDEX:" << (int)m_ReqDEX
        << "ReqINT:" << (int)m_ReqINT << "ReqSum:" << (int)m_ReqSum << "ReqLevel:" << (int)m_ReqLevel
        << "Level:" << (int)m_Level << "GambleLevel:" << (int)m_GambleLevel << "Color:" << (int)m_Color
        << "Ratio:" << (int)m_Ratio << "PreviousOptionType:" << (int)m_PreviousOptionType
        << "UpgradeOptionType:" << (int)m_UpgradeOptionType << "UpgradeRatio:" << (int)m_UpgradeRatio
        << "UpgradeSecondRatio:" << (int)m_UpgradeSecondRatio << "UpgradeCrashPercent:" << (int)m_UpgradeCrashPercent
        << "NextOptionRatio:" << (int)m_NextOptionRatio << ")";
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class OptionInfoSet
//////////////////////////////////////////////////////////////////////////////

OptionInfoSet::OptionInfoSet()

{}

OptionInfoSet::~OptionInfoSet()

{
    __BEGIN_TRY

    m_OptionTypes.clear();

    __END_CATCH_NO_RETHROW
}

// Put the option type into the map under the given level.
void OptionInfoSet::addOptionType(uint level, OptionType_t type)

{
    unordered_map<uint, OptionType_t>::iterator itr = m_OptionTypes.find(level);

    if (itr != m_OptionTypes.end()) {
        StringStream msg;
        msg << "DupOptionLevel(OptionInfoSet): level=" << (int)level << ", OptionType=" << (int)type;

        throw DuplicatedException(msg.toString().c_str());
    }

    m_OptionTypes[level] = type;
}

// Put the options within the given minimum and maximum level range into the vector.
void OptionInfoSet::getPossibleOptionTypes(uint minLevel, uint maxLevel, vector<OptionType_t>& rOptionVector)

{
    unordered_map<uint, OptionType_t>::iterator itr = m_OptionTypes.begin();
    for (; itr != m_OptionTypes.end(); itr++) {
        uint level = itr->first;
        if (minLevel <= level && level <= maxLevel) {
            OptionType_t OType = itr->second;
            rOptionVector.push_back(OType);
        }
    }
}

string OptionInfoSet::toString(void) const

{
    StringStream msg;

    msg << "OptionInfoSet(";

    unordered_map<uint, OptionType_t>::const_iterator itr = m_OptionTypes.begin();
    for (; itr != m_OptionTypes.end(); itr++)
        msg << (int)itr->first << ":" << (int)itr->second << ",";

    msg << ")\n";

    return msg.toString();
}


//////////////////////////////////////////////////////////////////////////////
// class OptionInfoManager member methods
//////////////////////////////////////////////////////////////////////////////

OptionInfoManager::OptionInfoManager()

{
    m_nOptionCount = 0;
    m_OptionClassInfos.clear();
    m_OptionClassInfos.reserve((int)OPTION_MAX);
    m_ToTalPetEnchantOption = 0;
    m_PetEnchantOptionList.clear();
}

OptionInfoManager::~OptionInfoManager()

{
    __BEGIN_TRY

    release();

    __END_CATCH_NO_RETHROW
}

void OptionInfoManager::release()

{
    m_nOptionCount = 0;

    unordered_map<OptionType_t, OptionInfo*>::iterator itr = m_OptionInfos.begin();
    for (; itr != m_OptionInfos.end(); itr++) {
        OptionInfo* pInfo = itr->second;
        SAFE_DELETE(pInfo);
    }
    m_OptionInfos.clear();

    m_NicknameOptionInfos.clear();

    for (int i = 0; i < OPTION_MAX; i++) {
        m_OptionInfoSet[i].clear();
    }

    for (int ic = 0; ic < Item::ITEM_CLASS_MAX; ic++) {
        for (uint l = 0; l < GAMBLE_OPTION_LEVEL_MAX; l++) {
            m_GambleOptions[ic][l].clear();
        }
    }

    vector<OptionClassInfo*>::iterator citr = m_OptionClassInfos.begin();
    vector<OptionClassInfo*>::iterator endcItr = m_OptionClassInfos.end();

    for (; citr != endcItr; ++citr) {
        if (*citr != NULL)
            SAFE_DELETE(*citr);
    }

    m_OptionClassInfos.clear();

    list<PetEnchantOption*>::iterator pitr = m_PetEnchantOptionList.begin();
    list<PetEnchantOption*>::iterator endpItr = m_PetEnchantOptionList.end();

    for (; pitr != endpItr; pitr++) {
        PetEnchantOption* pPetEnchantOption = *pitr;
        SAFE_DELETE(pPetEnchantOption);
    }
    m_PetEnchantOptionList.clear();
    m_ToTalPetEnchantOption = 0;
}

void OptionInfoManager::init()

{
    load();
}

void OptionInfoManager::load()

{
    release();

    try {
        vector<OptionInfoRow> options = defaultGameInfoRepository().loadOptionInfos();

        if (options.empty()) {
            throw Error("There is no data in OptionInfo Table");
        }

        for (size_t r = 0; r < options.size(); r++) {
            const OptionInfoRow& row = options[r];
            OptionInfo* pInfo = new OptionInfo;

            pInfo->setType(row.optionType);
            pInfo->setName(row.name);
            pInfo->setHName(row.hName);
            pInfo->setNickname(row.nickname);
            pInfo->setClass((OptionClass)row.optionClass);
            pInfo->setPlusPoint(row.plusPoint);
            pInfo->setPriceMultiplier(row.priceMultiplier);
            pInfo->setReqAbility(row.reqAbility);
            pInfo->setColor(row.color);
            pInfo->setRatio(row.ratio);
            pInfo->setLevel(row.optionLevel);
            pInfo->setGambleLevel(row.gambleLevel);
            pInfo->setPreviousType(row.previousOptionType);
            pInfo->setUpgradeType(row.upgradeOptionType);
            pInfo->setUpgradeRatio(row.upgradeRatio);
            pInfo->setUpgradeSecondRatio(row.upgradeSecondRatio);
            pInfo->setUpgradeCrashPercent(row.upgradeCrashPercent);
            pInfo->setNextOptionRatio(row.nextOptionRatio);
            pInfo->setGrade(row.grade);

            addOptionInfo(pInfo);

            // Put the level and type into the OptionInfoSet that matches the
            // option class.
            uint OClass = pInfo->getClass();
            uint level = pInfo->getLevel();
            OptionType_t type = pInfo->getType();
            Ratio_t gambleRatio = pInfo->getRatio();

            m_OptionInfoSet[OClass].addOptionType(level, type);

            // Set up the option information used for gambling.
            for (int ic = 0; ic < Item::ITEM_CLASS_MAX; ic++) {
                // If the option can be attached to this itemClass,
                // add it to m_GambleOptions.
                if (gambleRatio > 0 && isPossibleOptionClass((Item::ItemClass)ic, (OptionClass)OClass)) {
                    uint gambleLevel = pInfo->getGambleLevel();

                    // Add it up to that level.
                    for (uint l = gambleLevel; l <= GAMBLE_OPTION_LEVEL_MAX; l++) {
                        m_GambleOptions[ic][l].push_back(type);
                    }
                }
            }
        }

        vector<OptionClassInfoRow> classes = defaultGameInfoRepository().loadOptionClassInfos();

        if (classes.empty()) {
            throw Error("There is no data in OptionClassInfo Table");
        }

        for (size_t r = 0; r < classes.size(); r++) {
            OptionClassInfo* pInfo = new OptionClassInfo((OptionClass)classes[r].optionClassType);
            pInfo->setName(classes[r].name);
            pInfo->setHName(classes[r].hName);
            pInfo->setLevel(classes[r].level);
            pInfo->setTotalGrade(classes[r].totalGrade);
            pInfo->setOptionGroup((OptionGroup)classes[r].optionGroup);

            addOptionClassInfo(pInfo);
        }

        vector<RareEnchantRow> enchants = defaultGameInfoRepository().loadRareEnchantInfos();

        if (enchants.empty()) {
            throw Error("There is no data in RareEnchantInfo Table");
        }

        for (size_t r = 0; r < enchants.size(); r++) {
            int level = enchants[r].level;
            int total = enchants[r].totalGrade;

            int key = level * 10 + total;

            unordered_map<int, RareOptionUpgradeInfo*>::iterator itr = m_RareEnchantInfo.find(key);

            if (itr == m_RareEnchantInfo.end()) {
                RareOptionUpgradeInfo* pInfo = new RareOptionUpgradeInfo(level, total);
                m_RareEnchantInfo[key] = pInfo;
            }

            int grade = enchants[r].grade;

            m_RareEnchantInfo[key]->setRatio(grade, false, enchants[r].ratioWhenFail);
            m_RareEnchantInfo[key]->setRatio(grade, true, enchants[r].ratioWhenSuccess);
        }


        vector<PetEnchantOptionRatioRow> petRatios = defaultGameInfoRepository().loadPetEnchantOptionRatios();

        for (size_t r = 0; r < petRatios.size(); r++) {
            OptionType_t optionType = petRatios[r].optionType;
            int ratio = petRatios[r].ratio;

            ratio = ratio + getTotalPetEnchantOption();
            setTotalPetEnchantOption(ratio);

            PetEnchantOption* pPetEnchantOption = new PetEnchantOption();
            pPetEnchantOption->setOptionType(optionType);
            pPetEnchantOption->setRatio(ratio);

            addPetEnchantOption(pPetEnchantOption);
        }


        // Compute totalRatio for each gamble option.
        for (int ic = 0; ic < Item::ITEM_CLASS_MAX; ic++) {
            for (uint l = 0; l <= GAMBLE_OPTION_LEVEL_MAX; l++) {
                m_TotalGambleRatio[ic][l] = 0;

                const vector<OptionType_t>& optionVector = m_GambleOptions[ic][l];
                vector<OptionType_t>::const_iterator iOption;

                int totalRatio = 0;
                for (iOption = optionVector.begin(); iOption != optionVector.end(); iOption++) {
                    OptionType_t optionType = *iOption;

                    try {
                        OptionInfo* pOptionInfo = getOptionInfo(optionType);

                        if (pOptionInfo == NULL) {
                            cerr << "No Such Option : " << optionType << endl;
                            Assert(false);
                        }

                        totalRatio += pOptionInfo->getRatio();
                    } catch (NoSuchElementException&) {
                        cerr << "No Such Option : " << optionType << endl;
                        Assert(false);
                    }
                }

                m_TotalGambleRatio[ic][l] = totalRatio;
            }
        }

    } catch (Throwable& t) {
        cerr << t.toString().c_str() << endl;
        throw;
    }
}

void OptionInfoManager::addOptionInfo(OptionInfo* pOptionInfo)

{
    // Insert into the hash map keyed by OptionType.
    unordered_map<OptionType_t, OptionInfo*>::iterator itr = m_OptionInfos.find(pOptionInfo->getType());

    if (itr != m_OptionInfos.end()) {
        StringStream msg;
        msg << "DupOptionType: OptionType=" << (int)pOptionInfo->getType()
            << ", Nickname=" << pOptionInfo->getNickname();

        throw DuplicatedException(msg.toString());
    }

    m_OptionInfos[pOptionInfo->getType()] = pOptionInfo;

    // Insert into the hash map keyed by Nickname.
    unordered_map<string, OptionInfo*>::iterator itr2 = m_NicknameOptionInfos.find(pOptionInfo->getNickname());

    if (itr2 != m_NicknameOptionInfos.end()) {
        StringStream msg;
        msg << "DupNickname: OptionType=" << (int)pOptionInfo->getType() << ", Nickname=" << pOptionInfo->getNickname();

        throw DuplicatedException(msg.toString().c_str());
    }

    m_NicknameOptionInfos[pOptionInfo->getNickname()] = pOptionInfo;
}

OptionInfo* OptionInfoManager::getOptionInfo(OptionType_t OptionType)

{
    // cout << "getOptionInfo(" << (int)OptionType << ") " << endl;

    unordered_map<OptionType_t, OptionInfo*>::iterator itr = m_OptionInfos.find(OptionType);

    if (itr == m_OptionInfos.end()) {
        // cerr << "OptionInfoManager::getOptionInfo() : No Such Element Exception. OptionType=" << (int)OptionType <<
        // endl; throw NoSuchElementException();

        return NULL;
    }

    return itr->second;
}

OptionInfo* OptionInfoManager::getOptionInfo(const string& nickname)

{
    unordered_map<string, OptionInfo*>::iterator itr = m_NicknameOptionInfos.find(nickname);

    if (itr == m_NicknameOptionInfos.end()) {
        // cerr << "OptionInfoManager::getOptionInfo() : No Such Element Exception" << endl;
        // throw NoSuchElementException();

        return NULL;
    }

    return itr->second;
}

OptionType_t OptionInfoManager::getOptionType(const string& nickname)

{
    unordered_map<string, OptionInfo*>::iterator itr = m_NicknameOptionInfos.find(nickname);

    if (itr == m_NicknameOptionInfos.end()) {
        cerr << "OptionInfoManager::getOptionInfo() : No Such Element Exception" << endl;
        throw NoSuchElementException();
    }

    OptionInfo* pInfo = itr->second;
    return pInfo->getType();
}

string OptionInfoManager::getOptionName(const list<OptionType_t>& optionTypes)

{
    StringStream optionName;

    list<OptionType_t>::const_iterator itr;

    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        OptionInfo* pOptionInfo = getOptionInfo(*itr);
        optionName << pOptionInfo->getName() << " ";
    }

    return optionName.toString();
}


vector<OptionType_t> OptionInfoManager::getPossibleOptionVector(Item::ItemClass IClass, uint minLevel, uint maxLevel)

{
    vector<OptionType_t> temp;

    switch (IClass) {
    // WEAPON
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_SG:
    case Item::ITEM_CLASS_SMG:
    case Item::ITEM_CLASS_AR:
    case Item::ITEM_CLASS_SR:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
    case Item::ITEM_CLASS_VAMPIRE_WEAPON:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_TOHIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DAMAGE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ATTACK_SPEED].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);

        if (IClass != Item::ITEM_CLASS_VAMPIRE_WEAPON)
            m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);

        break;
    // ARMOR
    case Item::ITEM_CLASS_HELM:
    case Item::ITEM_CLASS_COAT:
    case Item::ITEM_CLASS_TROUSER:
    case Item::ITEM_CLASS_SHOES:
    case Item::ITEM_CLASS_GLOVE:
    case Item::ITEM_CLASS_BELT:
    case Item::ITEM_CLASS_SHIELD:
    case Item::ITEM_CLASS_VAMPIRE_COAT:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEFENSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_PROTECTION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);

        if (IClass != Item::ITEM_CLASS_VAMPIRE_COAT)
            m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);

        break;
    // ACCESSORY
    case Item::ITEM_CLASS_RING:
    case Item::ITEM_CLASS_BRACELET:
    case Item::ITEM_CLASS_NECKLACE:
    case Item::ITEM_CLASS_VAMPIRE_RING:
    case Item::ITEM_CLASS_VAMPIRE_EARRING:
    case Item::ITEM_CLASS_VAMPIRE_BRACELET:
    case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_REGEN].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_REGEN].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_TOHIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DAMAGE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEFENSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_PROTECTION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_POISON].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ACID].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CURSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_BLOOD].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_VISION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ATTACK_SPEED].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_RES].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);

        m_OptionInfoSet[OPTION_STR_TO_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_STR_TO_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX_TO_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX_TO_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT_TO_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT_TO_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);

        if (IClass != Item::ITEM_CLASS_VAMPIRE_EARRING)
            m_OptionInfoSet[OPTION_LUCK].getPossibleOptionTypes(minLevel, maxLevel, temp);

        if (IClass != Item::ITEM_CLASS_VAMPIRE_RING && IClass != Item::ITEM_CLASS_VAMPIRE_EARRING &&
            IClass != Item::ITEM_CLASS_VAMPIRE_BRACELET && IClass != Item::ITEM_CLASS_VAMPIRE_NECKLACE)
            m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);

        break;

    case Item::ITEM_CLASS_VAMPIRE_AMULET:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        //			m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_REGEN].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_REGEN].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_TOHIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DAMAGE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_POISON].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ACID].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CURSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_BLOOD].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_VISION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ATTACK_SPEED].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_LUCK].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_RES].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        break;

    case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
    case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_TOHIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DAMAGE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ATTACK_SPEED].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CRITICAL_HIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        break;

    case Item::ITEM_CLASS_OUSTERS_COAT:
    case Item::ITEM_CLASS_OUSTERS_BOOTS:
    case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEFENSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_PROTECTION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DURABILITY].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_POISON].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ACID].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CURSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_BLOOD].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_LUCK].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_RES].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        break;

    case Item::ITEM_CLASS_OUSTERS_CIRCLET:
    case Item::ITEM_CLASS_OUSTERS_PENDENT:
    case Item::ITEM_CLASS_OUSTERS_RING:
        m_OptionInfoSet[OPTION_STR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEX].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_INT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_HP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_MP_STEAL].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_TOHIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DAMAGE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_DEFENSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_PROTECTION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_POISON].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ACID].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CURSE].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_BLOOD].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_VISION].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ATTACK_SPEED].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_CRITICAL_HIT].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_LUCK].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_RES].getPossibleOptionTypes(minLevel, maxLevel, temp);
        m_OptionInfoSet[OPTION_ALL_ATTR].getPossibleOptionTypes(minLevel, maxLevel, temp);
        break;

    default:
        break;
    }

    return temp;
}

void OptionInfoManager::addGambleOption(Item::ItemClass itemClass, uint level, OptionType_t optionType)

{
    m_GambleOptions[itemClass][level].push_back(optionType);
}

int OptionInfoManager::getRareUpgradeRatio(OptionType_t optionType, bool success) {
    OptionInfo* pOI = getOptionInfo(optionType);
    if (pOI == NULL)
        throw Error("OptionInfoManager::getRareUpgradeRatio() : no option info.");

    OptionClassInfo* pOCI = m_OptionClassInfos[pOI->getClass()];
    if (pOCI == NULL)
        throw Error("OptionInfoManager::getRareUpgradeRatio() : no option class info.");

    RareOptionUpgradeInfo* pROUI = m_RareEnchantInfo[pOCI->getKey()];
    if (pROUI == NULL)
        throw Error("OptionInfoManager::getRareUpgradeRatio() : no rare enchant info.");

    return pROUI->getRatio(pOI->getGrade(), success);
}

void OptionInfoManager::addPetEnchantOption(PetEnchantOption* pPetEnchantOption)

{
    Assert(pPetEnchantOption != NULL);
    m_PetEnchantOptionList.push_back(pPetEnchantOption);
}


string OptionInfoManager::toString() const

{
    StringStream msg;
    msg << "OptionInfoManager(\n";

    for (uint i = 0; i < OPTION_MAX; i++)
        msg << m_OptionInfoSet[i].toString();

    msg << "\n)";
    return msg.toString();
}
