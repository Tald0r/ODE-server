//////////////////////////////////////////////////////////////////////////////
// Filename    : ActionPrepareShop.cpp
// Written By  :
// Description :
// Action that prepares the items a shop NPC will sell, run when the NPC is
// first loaded. See the ShopTemplate class and its manager.
//////////////////////////////////////////////////////////////////////////////

#include "ActionPrepareShop.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "Creature.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemFactoryManager.h"
#include "NPC.h"
#include "OptionInfo.h"
#include "ShopTemplate.h"
#include "repository/GameInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
ActionPrepareShop::ActionPrepareShop()

{
    m_ListNum = 0;
    m_MarketCondBuy = 100;
    m_MarketCondSell = 100;
    m_TaxingCastleZoneID = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
ActionPrepareShop::~ActionPrepareShop()

{
    try {
        clearList();
    } catch (Throwable&) {
        // A Throwable from the try block is dropped here and goes no further.
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ActionPrepareShop::read(PropertyBuffer& propertyBuffer)

{
    try {
        int NPCID = propertyBuffer.getPropertyInt("NPCID");

        vector<int> templateIDs = defaultGameInfoRepository().loadShopTemplateIDsOfNPC(NPCID);
        for (vector<int>::const_iterator it = templateIDs.begin(); it != templateIDs.end(); ++it) {
            ShopTemplateID_t id = *it;
            addListElement(id);
        }

        m_MarketCondBuy = propertyBuffer.getPropertyInt("MarketConditionBuy");
        m_MarketCondSell = propertyBuffer.getPropertyInt("MarketConditionSell");

    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


//////////////////////////////////////////////////////////////////////////////
// Execute the action.
// NOTE : Every ShopTemplate must be loaded before this action runs.
//////////////////////////////////////////////////////////////////////////////
void ActionPrepareShop::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);

    // IDList    : Lists that hold the ShopTemplate IDs, grouped by the
    //             kind of display rack.
    // combi[]   : Each class yields combinations from minItemType and maxItemType.
    //             Array holding the total number of those combinations.
    // count[]   : Number of items the NPC currently holds on the rack, per type.

    list<ShopTemplateID_t> IDList[SHOP_RACK_TYPE_MAX];
    int combi[SHOP_RACK_TYPE_MAX] = {0, 0, 0};
    int count[SHOP_RACK_TYPE_MAX] = {0, 0, 0};
    int trialMax = 0;
    ShopTemplate* pTemplate = NULL;
    ShopRackType_t shopType;
    int itemClass;
    ItemType_t minItemType, maxItemType, itemType;
    uint minOptionLevel, maxOptionLevel;
    OptionType_t optionType;

    // Each shop template has an item class and a minimum and maximum type.
    // The minimum and maximum type tell how many kinds of item
    // must be created. For example:
    // MinItemType : 0, MaxItemType : 0 --> 1 kind
    // MinItemType : 0, MaxItemType : 2 --> 3 kinds
    for (list<ShopTemplateID_t>::const_iterator itr = m_List.begin(); itr != m_List.end(); itr++) {
        // Take one template from the template list.
        pTemplate = context().shopTemplates().getTemplate((*itr));

        Assert(pTemplate != NULL);

        shopType = pTemplate->getShopType();
        itemClass = pTemplate->getItemClass();
        minItemType = pTemplate->getMinItemType();
        maxItemType = pTemplate->getMaxItemType();

        // Depending on the shop type (normal, special, ...)
        // put the ID of the shop template to create into the list,
        // and store the number of item kinds to create.
        IDList[shopType].push_back(*itr);
        combi[shopType] += (maxItemType - minItemType + 1);
    }

    // Create the items for each shop.
    for (ShopRackType_t i = 0; i < SHOP_RACK_TYPE_MAX; i++) {
        // From the combination count, work out how many items of one type in one
        // class can be placed on the rack.
        // ex) With 9 combinations in all, 20/9 = 3, so one item type of one
        // class can appear on the rack at most 3 times.
        if (combi[i] == 0)
            trialMax = 0;
        else
            trialMax = (int)(floor(SHOP_RACK_INDEX_MAX / combi[i]));

        // On the normal and mysterious racks several copies of the same item
        // are pointless, so the item is created only once.
        if (i == SHOP_RACK_NORMAL || i == SHOP_RACK_MYSTERIOUS)
            trialMax = 1;

        // Take the shop template IDs stored earlier one at a time and
        // create the shop items.
        for (list<ShopTemplateID_t>::const_iterator itr = IDList[i].begin(); itr != IDList[i].end(); itr++) {
            pTemplate = context().shopTemplates().getTemplate((*itr));
            itemClass = pTemplate->getItemClass();
            minItemType = pTemplate->getMinItemType();
            maxItemType = pTemplate->getMaxItemType();
            minOptionLevel = pTemplate->getMinOptionLevel();
            maxOptionLevel = pTemplate->getMaxOptionLevel();

            // First build the vector of option types that can be created.
            vector<OptionType_t> optionVector = context().optionInfos().getPossibleOptionVector(
                (Item::ItemClass)itemClass, minOptionLevel, maxOptionLevel);

            for (ItemType_t type = minItemType; type <= maxItemType; type++) {
                // One shop of a given type holds up to 20 items.
                // If 5 kinds of item must be created,
                // up to 4 of each item can be put in.
                // With 6 kinds, up to 3 of each can be put in.
                for (int tc = 0; tc < trialMax; tc++) {
                    itemType = type;

                    // Pick an option at random from the option vector.
                    // If no option is possible, optionType is 0 (no option).
                    Item::ItemClass IClass = Item::ItemClass(itemClass);
                    list<OptionType_t> optionTypes;
                    if (i != SHOP_RACK_MYSTERIOUS && optionVector.size() > 0) {
                        optionType = optionVector[(rand() % optionVector.size())];

                        if (optionType != 0)
                            optionTypes.push_back(optionType);
                    }

                    // Create the item itself.
                    Item* pItem = context().itemFactories().createItem(IClass, itemType, optionTypes);
                    Assert(pItem != NULL);

                    // Register it in the zone's object registry.
                    (pNPC->getZone()->getObjectRegistry()).registerObject(pItem);

                    // Add the item if the rack still has room.
                    if (count[i] < SHOP_RACK_INDEX_MAX) {
                        pNPC->insertShopItem(i, count[i], pItem);

                        count[i] = count[i] + 1;
                    }
                }
            }
        }
    }

    pNPC->setMarketCondBuy(m_MarketCondBuy);
    pNPC->setMarketCondSell(m_MarketCondSell);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void ActionPrepareShop::addListElement(ShopTemplateID_t id)

{
    m_List.push_back(id);
    m_ListNum++;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string ActionPrepareShop::toString() const

{
    StringStream msg;
    int i = 0;

    msg << "ActionPrepareShop(";
    list<ShopTemplateID_t>::const_iterator itr = m_List.begin();
    for (; itr != m_List.end(); itr++)
        msg << "Item" << i++ << ":" << (int)(*itr) << ",";
    msg << ")";

    return msg.toString();
}
