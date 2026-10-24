////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRegenShop.cpp
// Written By  :
// Description :
// Action that prepares the items a shop NPC will sell, run when the NPC is
// first loaded. See the ShopTemplate class and its manager.
////////////////////////////////////////////////////////////////////////////////

#include "ActionRegenShop.h"

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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ActionRegenShop::ActionRegenShop()

{
    m_Period.tv_sec = 0;
    m_Period.tv_usec = 0;
    m_NextRegen.tv_sec = 0;
    m_NextRegen.tv_usec = 0;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ActionRegenShop::~ActionRegenShop()

{
    clearList();
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionRegenShop::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read NPC id
        int NPCID = propertyBuffer.getPropertyInt("NPCID");

        vector<int> templateIDs = defaultGameInfoRepository().loadShopTemplateIDsOfNPC(NPCID);
        for (vector<int>::const_iterator it = templateIDs.begin(); it != templateIDs.end(); ++it) {
            ShopTemplateID_t id = *it;
            addListElement(id);
        }

        // Read the shop update period. (in seconds)
        int nSecond = propertyBuffer.getPropertyInt("Period");
        m_Period.tv_sec = nSecond;

        // Set when the next shop update is due.
        Timeval currentTime;
        getCurrentTime(currentTime);
        m_NextRegen = currentTime;
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
// NOTE : Every ShopTemplate must be loaded before this action runs.
////////////////////////////////////////////////////////////////////////////////
void ActionRegenShop::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    Zone* pZone = pNPC->getZone();
    Assert(pZone != NULL);

    // Get the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    // Return if it is not time to update yet.
    if (currentTime < m_NextRegen)
        return;

    // First check that no PC stands on a tile within five of the NPC.
    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);
    int centerX = pNPC->getX();
    int centerY = pNPC->getY();
    try {
        for (int zx = centerX - 5; zx <= centerX + 5; zx++) {
            for (int zy = centerY - 5; zy <= centerY + 5; zy++) {
                // Check that the coordinates are still in bounds...
                if (!rect.ptInRect(zx, zy)) {
                    continue;
                }

                Tile& tile = pZone->getTile(zx, zy);

                // Look for a walking creature
                if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                    Creature* pNearCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);
                    Assert(pNearCreature != NULL);
                    // Return if a PC is standing there
                    if (pNearCreature->isPC()) {
                        return;
                    }
                }
                // Look for a flying creature
                if (tile.hasCreature(Creature::MOVE_MODE_FLYING)) {
                    Creature* pNearCreature = tile.getCreature(Creature::MOVE_MODE_FLYING);
                    Assert(pNearCreature != NULL);
                    // Return if a PC is standing there
                    if (pNearCreature->isPC()) {
                        return;
                    }
                }
            }
        }
    } catch (Throwable& t) {
        filelog("shopbug.txt", "%s", t.toString().c_str());
        return;
    }

    try {
        // Drop every item the NPC holds and raise the shop version.

        pNPC->clearShopItem();

        for (int i = 0; i < SHOP_RACK_TYPE_MAX; i++)
            pNPC->increaseShopVersion(i);

        // Create the items.
        list<ShopTemplateID_t> IDList[SHOP_RACK_TYPE_MAX];
        int combi[SHOP_RACK_TYPE_MAX] = {0, 0, 0};
        int count[SHOP_RACK_TYPE_MAX] = {0, 0, 0};
        int trialMax = 0;
        ShopTemplate* pTemplate = NULL;
        ShopRackType_t shopType;
        int itemClass;
        ItemType_t minItemType, maxItemType, itemType;
        uint minOptionLevel, maxOptionLevel;
        OptionType_t optionType = 0;

        // Each shop template has an item class and a minimum and maximum type.
        // The minimum and maximum type tell how many kinds of the matching
        // item have to be created. For example:
        // MinItemType : 0, MaxItemType : 0 --> 1 kind
        // MinItemType : 0, MaxItemType : 2 --> 3 kinds
        for (list<ShopTemplateID_t>::const_iterator itr = m_List.begin(); itr != m_List.end(); itr++) {
            pTemplate = context().shopTemplates().getTemplate((*itr));

            Assert(pTemplate != NULL);

            shopType = pTemplate->getShopType();
            itemClass = pTemplate->getItemClass();
            minItemType = pTemplate->getMinItemType();
            maxItemType = pTemplate->getMaxItemType();

            // Depending on the shop type (normal, special, ...),
            // put the ID of the shop template to create into the list, and
            // store the number of item kinds to create.
            IDList[shopType].push_back(*itr);
            combi[shopType] += (maxItemType - minItemType + 1);
        }

        // Create the items for each shop.
        for (ShopRackType_t i = 0; i < SHOP_RACK_TYPE_MAX; i++) {
            // One shop type holds up to 20 items.
            // If 5 kinds of item have to be created,
            // up to 4 of each item can be put in.
            // With 6 kinds, up to 3 of each can be put in.
            // Store that number of attempts in the trialMax variable.
            if (combi[i] == 0)
                trialMax = 0;
            else
                trialMax = (int)(floor(SHOP_RACK_INDEX_MAX / combi[i]));

            // On the normal and mysterious racks several copies of the same item
            // are pointless, so the item is created only once.
            if (i == SHOP_RACK_NORMAL || i == SHOP_RACK_MYSTERIOUS)
                trialMax = 1;

            // Take the shop template IDs one by one from the list saved earlier
            // and create the shop items.
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
                    // One shop type holds up to 20 items.
                    // If 5 kinds of item have to be created,
                    // up to 4 of each item can be put in.
                    // With 6 kinds, up to 3 of each can be put in.
                    for (int tc = 0; tc < trialMax; tc++) {
                        itemType = type;

                        // Pick an option at random from the option vector.
                        // If no option is possible, optionType is 0 (no option).
                        if (i != SHOP_RACK_MYSTERIOUS && optionVector.size() > 0) {
                            int randValue = rand();
                            int size = optionVector.size();
                            int index = randValue % size;

                            optionType = optionVector[index];
                        } else
                            optionType = 0;

                        // Create the item itself.
                        Item::ItemClass IClass = Item::ItemClass(itemClass);
                        list<OptionType_t> optionTypes;

                        if (optionType != 0)
                            optionTypes.push_back(optionType);

                        Item* pItem = context().itemFactories().createItem(IClass, itemType, optionTypes);
                        Assert(pItem != NULL);

                        // Register in the zone's object registry.
                        (pZone->getObjectRegistry()).registerObject(pItem);

                        // Add the item if the rack has room.
                        if (count[i] < SHOP_RACK_INDEX_MAX) {
                            pNPC->insertShopItem(i, count[i], pItem);

                            count[i] = count[i] + 1;
                        }
                    }
                }
            }
        }

    } catch (Error& t) {
        // Errors are rethrown.
        filelog("regenShopBug.txt", "%s", t.toString().c_str());
        throw;
    } catch (Throwable& t) {
        filelog("regenShopBug.txt", "%s", t.toString().c_str());
    }

    // Set the time of the next update.
    m_NextRegen = m_NextRegen + m_Period;
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRegenShop::toString() const

{
    StringStream msg;
    int i = 0;

    msg << "ActionRegenShop(";

    list<ShopTemplateID_t>::const_iterator itr = m_List.begin();
    for (; itr != m_List.end(); itr++)
        msg << "Item" << i++ << ":" << (int)(*itr) << ",";

    msg << ")";

    return msg.toString();
}
