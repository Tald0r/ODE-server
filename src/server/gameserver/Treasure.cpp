//////////////////////////////////////////////////////////////////////////////
// Filename    : Treasure.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Treasure.h"

#include <string.h>

#include "Assert.h"
#include "GameContext.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "SXml.h"
#include "StringStream.h"
#include "UniqueItemManager.h"
#include "VariableManager.h"

const uint TREASURE_RATIO_MODULUS = 100000;

//////////////////////////////////////////////////////////////////////////////
// class TreasureOptionType member methods
//////////////////////////////////////////////////////////////////////////////

TreasureOptionType::TreasureOptionType()

{
    m_Ratio = 0;
    m_OptionType = 0;
}

TreasureOptionType::~TreasureOptionType()

{}

void TreasureOptionType::loadFromFile(ifstream& file)

{
    file.read((char*)&m_OptionType, sizeof(int));
    file.read((char*)&m_Ratio, sizeof(int));

    try {
        OptionInfo* pInfo = de::gameContext().optionInfos().getOptionInfo(m_OptionType);
        Assert(pInfo != NULL);
    } catch (NoSuchElementException& nsee) {
        cerr << "TreasureOptionType::loadFromFile() : Unknown Option" << endl;
        throw Error("TreasureOptionType::loadFromFile() : Unknown Option");
    }
}

void TreasureOptionType::parseString(const string& text)

{
    // cout << "OptionTypeText:" << text << endl;

    if (text.size() < 3)
        return;

    size_t a = text.find_first_of(',', 0);

    if (a != string::npos) {
        string optionString = trim(text.substr(0, a));
        m_Ratio = atoi(text.substr(a + 1, text.size() - a - 1).c_str());

        try {
            OptionInfo* pInfo = de::gameContext().optionInfos().getOptionInfo(optionString);
            Assert(pInfo != NULL);
            m_OptionType = pInfo->getType();
        } catch (NoSuchElementException& nsee) {
            cerr << "TreasureOptionType::parseString() : Unknown Option String[" << optionString << "]" << endl;
            throw Error("TreasureOptionType::parseString() : Unknown Option String");
        }
    } else {
        cerr << "TreasureOptionType::parseString() : Error[" << text << "]" << endl;
        throw Error("TreasureOptionType::parseString() : Error");
    }
}

string TreasureOptionType::toString(void) const

{
    StringStream msg;
    msg << "TreasureOptionType(" << "OptionType:" << m_OptionType << ",Ratio:" << m_Ratio << ")";
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class TreasureItemType member methods
//////////////////////////////////////////////////////////////////////////////

TreasureItemType::TreasureItemType()

{
    m_ItemType = 0;
    m_Ratio = 0;
    // add by sonic 2006.10.21;
    m_OptionRatioMax = 2;
    // end by sonic
}

TreasureItemType::~TreasureItemType()

{
    for (uint i = 0; i < m_TreasureOptionTypes.size(); i++) {
        TreasureOptionType* pTOT = m_TreasureOptionTypes[i];
        SAFE_DELETE(pTOT);
    }

    m_TreasureOptionTypes.clear();
}

void TreasureItemType::loadFromFile(int itemClass, ifstream& file)

{
    int OptionTypeCount = 0;

    file.read((char*)&m_ItemType, sizeof(int));
    file.read((char*)&m_Ratio, sizeof(int));
    file.read((char*)&OptionTypeCount, sizeof(int));

    list<OptionType_t> optionNULL;
    if (!de::gameContext().itemInfos().isPossibleItem((Item::ItemClass)itemClass, m_ItemType, optionNULL)) {
        StringStream msg;
        msg << "TreasureItemType::loadFromFile() : Invalid Item Type!\n"
            << "ItemClass:" << ItemClass2String[itemClass] << ",ItemType:" << (int)m_ItemType << "\n";
        cerr << msg.toString();
        throw msg.toString();
    }

    for (int i = 0; i < OptionTypeCount; i++) {
        TreasureOptionType* pTOT = new TreasureOptionType;
        pTOT->loadFromFile(file);
        m_TreasureOptionTypes.push_back(pTOT);
    }

    m_OptionTypeTotalRatio = 0;
    for (uint i = 0; i < m_TreasureOptionTypes.size(); i++) {
        TreasureOptionType* pTOT = m_TreasureOptionTypes[i];
        m_OptionTypeTotalRatio += pTOT->getRatio();
    }
}

void TreasureItemType::parseString(int itemClass, const string& text)

{
    ////////////////////////////////////////////////////////////
    // (1,50) (STR+1,50)(STR+2,30)(STR+30)
    ////////////////////////////////////////////////////////////

    size_t i = text.find_first_of('(', 0);
    size_t j = text.find_first_of(',', i + 1);
    size_t k = text.find_first_of(')', j + 1);

    if (i == string::npos || j == string::npos || k == string::npos) {
        cerr << "TreasureItemType::parseString() : Error" << endl;
        throw Error("TreasureItemType::parseString() : Error");
    }

    m_ItemType = atoi(trim(text.substr(i + 1, j - i - 1)).c_str());
    m_Ratio = atoi(trim(text.substr(j + 1, k - j - 1)).c_str());

    list<OptionType_t> optionNULL;
    if (!de::gameContext().itemInfos().isPossibleItem((Item::ItemClass)itemClass, m_ItemType, optionNULL)) {
        StringStream msg;
        msg << "TreasureItemType::parseString() : Invalid Item Type!\n"
            << "ItemClass:" << ItemClass2String[itemClass] << ",ItemType:" << (int)m_ItemType << "\n";
        cerr << msg.toString();
        throw msg.toString();
    }

    string newText = text.substr(k + 1, text.size() - k - 1);

    size_t a = 0;
    size_t b = 0;

    if (newText.size() > 0) {
        while (b < newText.size() - 1) {
            ////////////////////////////////////////////////////////////
            // (STR+1,50)(STR+2,30)(STR+30)
            ////////////////////////////////////////////////////////////
            a = newText.find_first_of('(', b);
            b = newText.find_first_of(')', a);

            if (a == string::npos || b == string::npos)
                break;

            string substring = trim(newText.substr(a + 1, b - a - 1));

            // cout << "TIT::substring\n" << substring << endl << endl;

            TreasureOptionType* pTOT = new TreasureOptionType;
            pTOT->parseString(substring);

            m_TreasureOptionTypes.push_back(pTOT);

            substring.clear();
        }
    }

    newText.clear();

    m_OptionTypeTotalRatio = 0;
    for (uint i = 0; i < m_TreasureOptionTypes.size(); i++) {
        TreasureOptionType* pTOT = m_TreasureOptionTypes[i];
        m_OptionTypeTotalRatio += pTOT->getRatio();
    }
}

bool TreasureItemType::getRandomOption(ITEM_TEMPLATE* pTemplate)

{
    if (m_OptionTypeTotalRatio == 0) {
        // pTemplate->OptionType.clear();
        // cout << "OptionTypeTotalRatio = 0" << endl;
        return false;
    }
    // for (int k=0; k< 2 ; k++)
    for (int k = 0; k < m_OptionRatioMax; k++) {
        bool bOneMore = false;

        int optionRatio = rand() % m_OptionTypeTotalRatio;
        int ratioSum = 0;

        // cout << "numOption=" << m_TreasureOptionTypes.size()
        //	 << ", Ratio=" << optionRatio << "/" << m_OptionTypeTotalRatio << endl;

        for (uint i = 0; i < m_TreasureOptionTypes.size(); i++) {
            TreasureOptionType* pTOT = m_TreasureOptionTypes[i];

            ratioSum += pTOT->getRatio();

            if (optionRatio < ratioSum) {
                if (pTemplate->bCreateOption) {
                    if (hasOptionClass(pTemplate->OptionType, pTOT->getOptionType())) {
                        // cout << "has Same OptionClass(" << pTOT->getOptionType() << ")" << endl;
                        return true;
                    }

                    if (pTOT->getOptionType() != 0) {
                        // cout << "add Option: " << pTOT->getOptionType() << endl;
                        pTemplate->OptionType.push_back(pTOT->getOptionType());
                    }
                }
                // else
                {
                    // pTemplate->OptionType = 0;
                }

                if (isPossibleNextOption(pTemplate)) {
                    bOneMore = true;
                    break;
                }

                return true;
            }
        }

        if (!bOneMore) {
            break;
        }
    }
    // while (1);

    // cout << "Cannot Select Option: "
    //	<< optionRatio << ", "
    //	<< ratioSum << endl;
    return false;
}

string TreasureItemType::toString(void) const

{
    StringStream msg;
    msg << "\tTreasureItemType\n\t\t\t(" << "\n\t\t\t\tItemType:" << m_ItemType << ",Ratio:" << m_Ratio << "\n";

    for (uint i = 0; i < m_TreasureOptionTypes.size(); i++) {
        msg << "\t\t\t\t" << m_TreasureOptionTypes[i]->toString() << "\n";
    }

    msg << "\n\t\t\t)";
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class TreasureItemClass member methods
//////////////////////////////////////////////////////////////////////////////

TreasureItemClass::TreasureItemClass()

{
    m_ItemClass = Item::ITEM_CLASS_MAX;
    m_Ratio = 0;

    // add By Sonic 2006.10.21
    m_OptionRatioMax = 2;
    // end by sonic
}

TreasureItemClass::~TreasureItemClass()

{
    for (uint i = 0; i < m_TreasureItemTypes.size(); i++) {
        TreasureItemType* pTIT = m_TreasureItemTypes[i];
        SAFE_DELETE(pTIT);
    }

    m_TreasureItemTypes.clear();
}

void TreasureItemClass::loadFromFile(ifstream& file)

{
    int ItemTypeCount = 0;
    file.read((char*)&m_ItemClass, sizeof(int));
    file.read((char*)&m_Ratio, sizeof(int));
    file.read((char*)&ItemTypeCount, sizeof(int));

    for (int i = 0; i < ItemTypeCount; i++) {
        TreasureItemType* pTIT = new TreasureItemType;
        pTIT->loadFromFile(m_ItemClass, file);
        m_TreasureItemTypes.push_back(pTIT);
    }

    m_ItemTypeTotalRatio = 0;
    for (uint i = 0; i < m_TreasureItemTypes.size(); i++) {
        TreasureItemType* pTIT = m_TreasureItemTypes[i];
        m_ItemTypeTotalRatio += pTIT->getRatio();
    }
}

void TreasureItemClass::parseString(const string& text)

{
    ////////////////////////////////////////////////////////////
    // (SWORD, 50)
    // <(1,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    // <(2,30) (STR+1,50)(STR+2,30)(STR+3,20)>
    // <(3,20) (STR+1,50)(STR+2,30)(STR+3,20)>
    ////////////////////////////////////////////////////////////

    size_t i = text.find_first_of('(', 0);
    size_t j = text.find_first_of(',', i + 1);
    size_t k = text.find_first_of(')', j + 1);

    string itemClassString = trim(text.substr(i + 1, j - i - 1));
    m_Ratio = atoi(trim(text.substr(j + 1, k - j - 1)).c_str());

    m_ItemClass = getItemClassFromString(itemClassString);

    string newText = text.substr(k + 1, text.size() - k - 1);

    // cout << "TIC::newText\n" << newText << endl << endl;

    size_t a = 0;
    size_t b = 0;

    while (b < newText.size() - 1) {
        ////////////////////////////////////////////////////////////
        // <(1,50,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(2,30,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(3,20,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        ////////////////////////////////////////////////////////////
        a = newText.find_first_of('<', b);
        b = newText.find_first_of('>', a);

        if (a == string::npos || b == string::npos)
            break;

        string substring = trim(newText.substr(a + 1, b - a - 1));

        // cout << "TIC::substring\n" << substring << endl << endl;

        TreasureItemType* pTIT = new TreasureItemType;
        pTIT->parseString(m_ItemClass, substring);

        m_TreasureItemTypes.push_back(pTIT);

        substring.clear();
    }

    newText.clear();

    m_ItemTypeTotalRatio = 0;
    for (uint i = 0; i < m_TreasureItemTypes.size(); i++) {
        TreasureItemType* pTIT = m_TreasureItemTypes[i];
        m_ItemTypeTotalRatio += pTIT->getRatio();
    }
}

bool TreasureItemClass::getRandomItem(ITEM_TEMPLATE* pTemplate)

{
    int itemTypeRatio = rand() % TREASURE_RATIO_MODULUS;
    int ratioSum = 0;

    for (uint i = 0; i < m_TreasureItemTypes.size(); i++) {
        TreasureItemType* pTIT = m_TreasureItemTypes[i];
        ratioSum += pTIT->getRatio();

        if (itemTypeRatio < ratioSum) {
            pTemplate->ItemType = pTIT->getItemType();

            ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pTemplate->ItemClass, pTemplate->ItemType);
            Assert(pItemInfo != NULL);

            if (pItemInfo->isUnique()) {
                int okRatio = rand() % 10000;
                int uniqueRatio = de::gameContext().variables().getUniqueItemRatio();

                if (okRatio < uniqueRatio &&
                    UniqueItemManager::isPossibleCreate(pTemplate->ItemClass, pTemplate->ItemType)) {
                    return true;
                }

                return false;
            } else {
                // add by Sonic 2006.10.21
                pTIT->setRndItemOptionMax(getRndItemOptionMax());
                // end by Sonic
                pTIT->getRandomOption(pTemplate);
                return true;
            }
        }
    }

    return false;
}

string TreasureItemClass::toString(void) const

{
    StringStream msg;
    msg << "\tTreasureItemClass\n\t\t(" << "\n\t\t\tItemClass:" << m_ItemClass << ",Ratio:" << m_Ratio << "\n";

    for (uint i = 0; i < m_TreasureItemTypes.size(); i++) {
        msg << "\t\t" << m_TreasureItemTypes[i]->toString() << "\n";
    }

    msg << "\n\t\t)";
    return msg.toString();
}

Item::ItemClass TreasureItemClass::getItemClassFromString(const string& text)

{
    for (int i = 0; i < Item::ITEM_CLASS_MAX; i++) {
        if (strcasecmp(text.c_str(), ItemClass2ShortString[i].c_str()) == 0) {
            return (Item::ItemClass)i;
        }
    }

    StringStream msg;
    msg << "TreasureItemClass::getItemClassFromString() : Unknown String[" << text << "]";
    cerr << msg.toString() << endl;
    throw msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class Treasure member methods
//////////////////////////////////////////////////////////////////////////////

Treasure::Treasure()

{
    m_ItemRatio = 0;
    m_OptionRatio = 0;
    m_ItemClassTotalRatio = 0;
    // add by sonic 2006.10.21;
    m_OptionRatioMax = 2;
}

Treasure::~Treasure()

{
    for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
        TreasureItemClass* pTIC = m_TreasureItemClasses[i];
        SAFE_DELETE(pTIC);
    }

    m_TreasureItemClasses.clear();
}

void Treasure::loadFromFile(ifstream& file)

{
    int ItemClassCount = 0;

    file.read((char*)&m_ItemRatio, sizeof(int));
    file.read((char*)&m_OptionRatio, sizeof(int));
    file.read((char*)&ItemClassCount, sizeof(int));

    for (int i = 0; i < ItemClassCount; i++) {
        TreasureItemClass* pTIC = new TreasureItemClass;
        pTIC->loadFromFile(file);
        m_TreasureItemClasses.push_back(pTIC);
    }

    m_ItemClassTotalRatio = 0;
    for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
        TreasureItemClass* pTIC = m_TreasureItemClasses[i];
        m_ItemClassTotalRatio += pTIC->getRatio();
    }
}

void Treasure::parseString(const string& text)

{
    ////////////////////////////////////////////////////////////
    // (50,50)
    // BEGIN_ITEM_CLASS (SWORD, 50)
    // <(1,50)(STR+1,50)(STR+2,30)(STR+3,20)>
    // <(2,30)(STR+1,50)(STR+2,30)(STR+3,20)>
    // <(3,20)(STR+1,50)(STR+2,30)(STR+3,20)>
    // END_ITEM_CLASS
    // BEGIN_ITEM_CLASS (BLADE, 50)
    // <(1,50)(STR+1,50)(STR+2,30)(STR+3,20)>
    // <(2,30)(STR+1,50)(STR+2,30)(STR+3,20)>
    // <(3,20)(STR+1,50)(STR+2,30)(STR+3,20)>
    // END_ITEM_CLASS
    ////////////////////////////////////////////////////////////

    size_t i = text.find_first_of('(');
    size_t j = text.find_first_of(',');
    size_t k = text.find_first_of(')');

    if (i == string::npos || j == string::npos || k == string::npos) {
        cerr << "Treasure::parseString() : Error" << endl;
        throw Error("Treasure::parseString() : Error");
    }

    // m_ItemRatio = atoi(trim(text.substr(i+1, j-i-1)).c_str()) * 150 / 100 ;
    m_ItemRatio = atoi(trim(text.substr(i + 1, j - i - 1)).c_str());

    // m_ItemRatio = (int)((float)atoi(trim(text.substr(i+1, j-i-1)).c_str()) * 1.5);

    // m_OptionRatio = atoi(trim(text.substr(j+1, k-j-1)).c_str()) * 150 / 100;
    m_OptionRatio = atoi(trim(text.substr(j + 1, k - j - 1)).c_str());

    string newText = trim(text.substr(k + 1, text.size() - k - 1));

    string btoken = "BEGIN_ITEMCLASS";
    string etoken = "END_ITEMCLASS";
    uint BTOKENSIZE = btoken.size();

    size_t a = 0;
    size_t b = 0;

    while (b < newText.size() - 1) {
        ////////////////////////////////////////////////////////////
        // BEGIN_ITEM_CLASS (SWORD, 50)
        // <(1,50,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(2,30,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(3,20,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // END_ITEM_CLASS
        // BEGIN_ITEM_CLASS (BLADE, 50)
        // <(1,50,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(2,30,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // <(3,20,50) (STR+1,50)(STR+2,30)(STR+3,20)>
        // END_ITEM_CLASS
        ////////////////////////////////////////////////////////////
        a = newText.find(btoken, b);
        b = newText.find(etoken, a + 1);

        if (a == string::npos || b == string::npos)
            break;

        string substring = trim(newText.substr(a + BTOKENSIZE, b - a - BTOKENSIZE));

        TreasureItemClass* pTIC = new TreasureItemClass;
        pTIC->parseString(substring);

        m_TreasureItemClasses.push_back(pTIC);

        substring.clear();
    }

    newText.clear();

    m_ItemClassTotalRatio = 0;
    for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
        TreasureItemClass* pTIC = m_TreasureItemClasses[i];
        m_ItemClassTotalRatio += pTIC->getRatio();
    }
}

bool Treasure::getRandomItem(ITEM_TEMPLATE* pTemplate)

{
    if (0 == m_ItemClassTotalRatio)
        return false;

    int ItemRatio = rand() % TREASURE_RATIO_MODULUS;


    double EventMultiplier = 0.0;
    if (de::gameContext().variables().getEventActivate() == 1)
        EventMultiplier = de::gameContext().variables().getItemProbRatio() / 100;
    else
        EventMultiplier = 1;

    if (ItemRatio < m_ItemRatio * EventMultiplier) {
        int OptionRatio = rand() % TREASURE_RATIO_MODULUS;

        if (OptionRatio < m_OptionRatio) {
            pTemplate->bCreateOption = true;
        } else {
            pTemplate->bCreateOption = false;
        }


        int itemClassRatio = rand() % m_ItemClassTotalRatio;
        int ratioSum = 0;
        int preRatioSum = 0;


        for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
            TreasureItemClass* pTIC = m_TreasureItemClasses[i];

            preRatioSum = ratioSum;
            ratioSum += pTIC->getRatio();

            if (itemClassRatio > preRatioSum && itemClassRatio < ratioSum) {
                pTemplate->ItemClass = pTIC->getItemClass();
                if (pTIC->getRandomItem(pTemplate))
                    return true;
                else
                    return false;
            }
        }
    }

    return false;
}

bool Treasure::getRandomItem(ITEM_TEMPLATE* pTemplate, int nPercent)

{
    int ItemRatio = rand() % TREASURE_RATIO_MODULUS;

    int ModifyItemRatio = getPercentValue(m_ItemRatio, nPercent);

    if (ItemRatio < ModifyItemRatio) {
        int OptionRatio = rand() % TREASURE_RATIO_MODULUS;

        if (OptionRatio < m_OptionRatio) {
            pTemplate->bCreateOption = true;
        } else {
            pTemplate->bCreateOption = false;
        }


        if (m_ItemClassTotalRatio == 0)
            return false;

        int itemClassRatio = rand() % m_ItemClassTotalRatio;
        int ratioSum = 0;
        int preRatioSum = 0;


        for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
            TreasureItemClass* pTIC = m_TreasureItemClasses[i];

            // end by sonic
            preRatioSum = ratioSum;
            ratioSum += pTIC->getRatio();

            if (itemClassRatio > preRatioSum && itemClassRatio < ratioSum) {
                pTemplate->ItemClass = pTIC->getItemClass();
                // add by Sonic 2006.10.21
                pTIC->setRndItemOptionMax(getRndItemOptionMax());
                // end by sonic
                if (pTIC->getRandomItem(pTemplate))
                    return true;
                else
                    return false;
            }
        }
    }

    return false;
}

string Treasure::toString(void) const

{
    StringStream msg;
    msg << "\tTreasure\n\t(" << "\n\t\tItemRatio:" << m_ItemRatio << "\n\t\tOptionRatio:" << m_ItemRatio << "\n";

    for (uint i = 0; i < m_TreasureItemClasses.size(); i++) {
        msg << "\t" << m_TreasureItemClasses[i]->toString() << "\n";
    }

    msg << "\n\t)";
    return msg.toString();
}

//////////////////////////////////////////////////////////////////////////////
// class TreasureList member methods
//////////////////////////////////////////////////////////////////////////////

TreasureList::TreasureList()

{}

TreasureList::~TreasureList()

{}

void TreasureList::loadFromFile(ifstream& file)

{
    list<Treasure*>::iterator itr = m_Treasures.begin();
    for (; itr != m_Treasures.end(); itr++) {
        Treasure* pTreasure = *itr;
        SAFE_DELETE(pTreasure);
    }
    m_Treasures.clear();


    int TreasureCount = 0;
    file.read((char*)&TreasureCount, sizeof(int));

    for (int i = 0; i < TreasureCount; i++) {
        Treasure* pTreasure = new Treasure;
        pTreasure->loadFromFile(file);
        addTreasure(pTreasure);
    }
}

void TreasureList::parseString(const string& text)

{
    ////////////////////////////////////////////////////////////
    // BEGIN_TREASURE (50)
    //   BEGIN_ITEM_CLASS (SWORD, 50)
    //     <(1,50,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //     <(2,30,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //     <(3,20,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //   END_ITEM_CLASS
    //   BEGIN_ITEM_CLASS (BLADE, 50)
    //     <(2,50,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //     <(3,30,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //     <(4,20,50) (STR+1,50)(STR+2,30)(STR+3,20)>
    //   END_ITEM_CLASS
    // END_TREASURE
    ////////////////////////////////////////////////////////////

    string bToken = "BEGIN_TREASURE";
    string eToken = "END_TREASURE";
    uint BTOKENSIZE = bToken.size();

    size_t a = 0;
    size_t b = 0;

    while (b < text.size()) {
        a = text.find(bToken, b);
        b = text.find(eToken, a + 1);

        if (a == string::npos || b == string::npos)
            break;

        string substring = trim(text.substr(a + BTOKENSIZE, b - a - BTOKENSIZE));

        // cout << "TreasureList SubString:\n" << substring << endl << endl;

        Treasure* pTreasure = new Treasure;
        pTreasure->parseString(substring);
        addTreasure(pTreasure);

        substring.clear();
    }
}

void TreasureList::addTreasure(Treasure* pTreasure)

{
    Assert(pTreasure != NULL);
    m_Treasures.push_back(pTreasure);
}

string TreasureList::toString(void) const

{
    StringStream msg;
    msg << "TreasureList\n(\n";

    list<Treasure*>::const_iterator itr = m_Treasures.begin();
    for (; itr != m_Treasures.end(); itr++) {
        msg << (*itr)->toString() << "\n";
    }

    msg << "\n)";
    return msg.toString();
}

TreasureLists::TreasureLists() {}

TreasureLists::~TreasureLists() {
    clear();
}

void TreasureLists::clear() {
    unordered_map<string, TreasureList*>::iterator itr = m_TreasureLists.begin();
    for (; itr != m_TreasureLists.end(); itr++) {
        TreasureList* pTreasureList = itr->second;
        SAFE_DELETE(pTreasureList);
    }
    m_TreasureLists.clear();
}

TreasureList* TreasureLists::getTreasure(const string& filename) const {
    unordered_map<string, TreasureList*>::const_iterator itr = m_TreasureLists.find(filename);

    if (itr != m_TreasureLists.end()) {
        return itr->second;
    }

    return NULL;
}

void TreasureLists::addTreasure(const string& filename, TreasureList* pTreasureList) {
    unordered_map<string, TreasureList*>::const_iterator itr = m_TreasureLists.find(filename);

    if (itr != m_TreasureLists.end()) {
        TreasureList* pTreasureList = itr->second;
        SAFE_DELETE(pTreasureList);
    }

    m_TreasureLists[filename] = pTreasureList;
}

TreasureList* TreasureLists::loadTreasure(const string& filename) {
    TreasureList* pTreasureList = new TreasureList;
    ifstream file(filename.c_str(), ios::in | ios::binary);

    if (!file) {
        StringStream msg;
        msg << "Cannot open " << filename << " to read.";
        cerr << msg.toString() << endl;
        throw msg.toString();
    }

    pTreasureList->loadFromFile(file);
    file.close();

    addTreasure(filename, pTreasureList);

    return pTreasureList;
}
