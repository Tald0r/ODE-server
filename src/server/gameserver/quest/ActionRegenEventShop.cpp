////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionRegenEventShop.cpp
// Written By  : excel96
// Description :
// Action that refreshes the shop for the Christmas event
////////////////////////////////////////////////////////////////////////////////

#include "ActionRegenEventShop.h"

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
ActionRegenEventShop::ActionRegenEventShop()

{
    __BEGIN_TRY

    m_Period.tv_sec = 0;
    m_Period.tv_usec = 0;
    m_NextRegen.tv_sec = 0;
    m_NextRegen.tv_usec = 0;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
ActionRegenEventShop::~ActionRegenEventShop()

{
    __BEGIN_TRY

    clearList();

    __END_CATCH_NO_RETHROW
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionRegenEventShop::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY

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

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
// NOTE : Every ShopTemplate must be loaded before this action runs.
////////////////////////////////////////////////////////////////////////////////
void ActionRegenEventShop::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    Assert(pCreature1 != NULL);
    Assert(pCreature1->isNPC());

    NPC* pNPC = dynamic_cast<NPC*>(pCreature1);
    Assert(pNPC != NULL);

    // Mark the NPC as an event shop.
    pNPC->setShopType(SHOPTYPE_EVENT);

    Zone* pZone = pNPC->getZone();
    Assert(pZone != NULL);

    // Get the current time.
    Timeval currentTime;
    getCurrentTime(currentTime);

    // Return if it is not time to update yet.
    if (currentTime < m_NextRegen)
        return;

    // Check first that no PC is standing near the NPC.
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

    // Drop every item the NPC holds and bump the shop version.
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
    OptionType_t optionType;

    // Each shop template has an item class and a minimum and maximum type.
    // The minimum and maximum type tell how many kinds of item
    // must be created. For example:
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
        // put the ID of the shop template to create into the list,
        // and store the number of item kinds to create.
        IDList[shopType].push_back(*itr);
        combi[shopType] += (maxItemType - minItemType + 1);
    }

    // Create the items for each shop.
    for (ShopRackType_t i = 0; i < SHOP_RACK_TYPE_MAX; i++) {
        // One shop of a given type holds up to 20 items.
        // If 5 kinds of item must be created,
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
        // create the shop items.
        for (list<ShopTemplateID_t>::const_iterator itr = IDList[i].begin(); itr != IDList[i].end(); itr++) {
            pTemplate = context().shopTemplates().getTemplate((*itr));
            itemClass = pTemplate->getItemClass();
            minItemType = pTemplate->getMinItemType();
            maxItemType = pTemplate->getMaxItemType();

            // First build the vector of option types that can be created.
            // If ItemType from the ShopTemplate is 2 or 3, optionType is limited to +2,
            // and higher ItemTypes get higher option levels.

            vector<OptionType_t> optionVector;
            if (minItemType == 2 && maxItemType == 3) {
                optionVector.push_back(2);  // STR+2
                optionVector.push_back(7);  // DEX+2
                optionVector.push_back(12); // INT+2
                optionVector.push_back(79); // ASPEED+2
            } else if (minItemType == 4 && maxItemType == 5) {
                optionVector.push_back(3);  // STR+3
                optionVector.push_back(8);  // DEX+3
                optionVector.push_back(13); // INT+3
                optionVector.push_back(80); // ASPEED+3
            } else if (minItemType == 6 && maxItemType == 6) {
                optionVector.push_back(4);  // STR+4
                optionVector.push_back(9);  // DEX+4
                optionVector.push_back(14); // INT+4
                optionVector.push_back(80); // ASPEED+3
            }

            for (ItemType_t type = minItemType; type <= maxItemType; type++) {
                // One shop type holds up to 20 items.
                // If 5 kinds of item have to be created,
                // up to 4 of each item can be put in.
                // With 6 kinds, up to 3 of each can be put in.
                for (int tc = 0; tc < trialMax; tc++) {
                    itemType = type;

                    // Pick an option at random from the option vector.
                    // If no option is possible, optionType is 0 (no option).
                    if (optionVector.size() > 0) {
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

                    // Register it in the zone's object registry.
                    (pZone->getObjectRegistry()).registerObject(pItem);

                    // Add the item if the rack still has room.
                    if (count[i] < SHOP_RACK_INDEX_MAX) {
                        pNPC->insertShopItem(i, count[i], pItem);
                        count[i] = count[i] + 1;
                    }
                }
            }
        }
    }

    // Set the time of the next update.
    m_NextRegen = m_NextRegen + m_Period;

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionRegenEventShop::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    int i = 0;

    msg << "ActionRegenEventShop(";
    list<ShopTemplateID_t>::const_iterator itr = m_List.begin();
    for (; itr != m_List.end(); itr++)
        msg << "Item" << i++ << ":" << (int)(*itr) << ",";
    msg << ")";

    return msg.toString();

    __END_CATCH
}
