//--------------------------------------------------------------------
//
// Filename    : InfoClassManager.cpp
// Written By  : Elca
//
//--------------------------------------------------------------------

// include files
#include "InfoClassManager.h"

#include "GameContext.h"
#include "ItemInfo.h"
#include "VariableManager.h"

//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
InfoClassManager::InfoClassManager()

    : m_InfoCount(0), m_pItemInfos(NULL), m_TotalRatio(0), m_AveragePrice(0) {}


//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
InfoClassManager::~InfoClassManager()

{
    if (m_pItemInfos != NULL) {
        for (uint i = 0; i <= m_InfoCount; i++)
            SAFE_DELETE(m_pItemInfos[i]);

        SAFE_DELETE_ARRAY(m_pItemInfos);
    }
}


//--------------------------------------------------------------------------------
// init
//--------------------------------------------------------------------------------
void InfoClassManager::init()

{
    load();

    // Assert(m_InfoCount>0);

    m_TotalRatio = 0;
    m_AveragePrice = 0;
    int count = 0;

    for (uint i = 0; i <= m_InfoCount; i++) {
        Ratio_t itemRatio = m_pItemInfos[i]->getRatio();

        if (itemRatio > 0) {
            // Compute the total ratio of the item types.
            m_TotalRatio += itemRatio;

            // Sum of the prices
            m_AveragePrice += m_pItemInfos[i]->getPrice();

            count++;
        }
    }

    // Average price
    if (count > 1) {
        m_AveragePrice /= count;
    }

    Assert(m_pItemInfos[0] != NULL);

    // Price increment
    m_AveragePrice /= 1000;
    m_AveragePrice *= 100;
}

//--------------------------------------------------------------------------------
// reload item infos
//--------------------------------------------------------------------------------
void InfoClassManager::reload()

{
    removeAllItemInfo();

    init();
}

//--------------------------------------------------------------------------------
// add item info
//--------------------------------------------------------------------------------
void InfoClassManager::addItemInfo(ItemInfo* pItemInfo)

{
    Assert(pItemInfo != NULL);
    Assert(pItemInfo->getItemType() < Item::ITEM_CLASS_MAX);
    Assert(m_pItemInfos[pItemInfo->getItemType()] == NULL);

    m_pItemInfos[pItemInfo->getItemType()] = pItemInfo;
}


//--------------------------------------------------------------------------------
// get item info
//--------------------------------------------------------------------------------
ItemInfo* InfoClassManager::getItemInfo(ItemType_t itemType) const {
    // Assert(itemType < Item::ITEM_CLASS_MAX);
    Assert(m_pItemInfos[itemType] != NULL);

    return m_pItemInfos[itemType];
}

//--------------------------------------------------------------------------------
// remove all item infos
//--------------------------------------------------------------------------------
void InfoClassManager::removeAllItemInfo()

{
    if (m_pItemInfos != NULL) {
        for (uint i = 0; i <= m_InfoCount; i++)
            SAFE_DELETE(m_pItemInfos[i]);
    }
}

//--------------------------------------------------------------------------------
// get Random ItemType
//--------------------------------------------------------------------------------
ItemType_t InfoClassManager::getRandomItemType() const

{
    // The DB may hold wrong values, so this has to be checked.
    if (m_TotalRatio == 0 || m_InfoCount == 0)
        return 0;

    int gambleRatio = de::gameContext().variables().getGambleItemTypeRatio(); // 200%
    int failRatio = m_pItemInfos[0]->getRatio(); // The ratio of item 0 is the failure probability.
    int succeedRatio = m_TotalRatio - failRatio; // Everything but item 0 is the success probability.
    int newTotalRatio = failRatio + getPercentValue(succeedRatio, gambleRatio); // fail + success*gambleRatio
    int itemTypeRatio = rand() % newTotalRatio;
    int ratio;
    int ratioSum = 0;

    // Item 0 counts as a failure.
    for (uint i = 0; i <= m_InfoCount; i++) {
        ItemInfo* pInfo = m_pItemInfos[i];
        ratio = pInfo->getRatio();

        // gambleRatio is applied only when the index is not 0.
        // Index 0 is the failure item; only the rest have their probability raised.
        if (i != 0) {
            // cout << "[" << i << "] " << ratio;
            ratio = getPercentValue(ratio, gambleRatio);

            // cout << " --> " << ratio;
        } else {
            // cout << "[" << i << "] " << ratio;
        }

        ratioSum += ratio;

        // cout << " , ratioSum/Select = " << ratioSum << "/" << itemTypeRatio << endl;

        if (itemTypeRatio < ratioSum) {
            // Select the i-th type. It is probably pInfo->getItemType()==i.
            return pInfo->getItemType();
        }
    }

    // Can this happen?
    // It can: getPercentValue may round the total differently from each entry.
    return 0;
}

//--------------------------------------------------------------------------------
// toString for debug
//--------------------------------------------------------------------------------
string InfoClassManager::toString() const

{
    StringStream msg;

    msg << "InfoClassManager(";

    for (uint i = 0; i <= m_InfoCount; i++) {
        msg << m_pItemInfos[i]->toString();
    }

    msg << ")";

    return msg.toString();
}
