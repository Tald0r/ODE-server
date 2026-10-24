//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUsePowerPointHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUsePowerPoint.h"

#ifdef __GAME_SERVER__
#include "Assert.h"
#include "GCCreateItem.h"
#include "GCUsePowerPointResult.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "PlayerCreature.h"
#include "Zone.h"
#include "mofus/Mofus.h"

struct POWER_POINT_ITEM_TEMPLATE {
    GCUsePowerPointResult::ITEM_CODE ItemCode;
    Item::ItemClass ItemClass;
    ItemType_t ItemType;
    uint Ratio;
    string ItemName;
};
// change the gambling system items
const POWER_POINT_ITEM_TEMPLATE PowerPointItemTemplate[7] = {
    {GCUsePowerPointResult::CANDY, Item::ITEM_CLASS_EVENT_ETC, 14, 20, "Elixir Pill"},
    {GCUsePowerPointResult::RESURRECTION_SCROLL, Item::ITEM_CLASS_RESURRECT_ITEM, 0, 20, "Resurrection Scroll"},
    {GCUsePowerPointResult::ELIXIR_SCROLL, Item::ITEM_CLASS_RESURRECT_ITEM, 1, 20, "Elixir Scroll"},
    {GCUsePowerPointResult::MEGAPHONE, Item::ITEM_CLASS_EFFECT_ITEM, 0, 20, "Megaphone"},
    {GCUsePowerPointResult::NAMING_PEN, Item::ITEM_CLASS_EVENT_GIFT_BOX, 22, 14, "Nickname Signing Pen"},
    {GCUsePowerPointResult::SIGNPOST, Item::ITEM_CLASS_EVENT_STAR, 6, 1, "Sapphire (Football)"},
    {GCUsePowerPointResult::BLACK_RICE_CAKE_SOUP, Item::ITEM_CLASS_EVENT_STAR, 11, 5, "Black Blessing"}

};
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUsePowerPointHandler::execute(CGUsePowerPoint* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);


    cout << "--------------------------------------------------" << endl;
    cout << "RECV UsePowerPoint (Name:" << pCreature->getName() << ")" << endl;
    cout << "--------------------------------------------------" << endl;


#ifdef __MOFUS__

    GCUsePowerPointResult gcUsePowerPointResult;

    // Check that there are power points
    if (pPC->getPowerPoint() < 300) {
        // There are not enough power points.
        gcUsePowerPointResult.setErrorCode(GCUsePowerPointResult::NOT_ENOUGH_POWER_POINT);

        pGamePlayer->sendPacket(&gcUsePowerPointResult);

        return;
    }

    // Check that there is room in the inventory.
    _TPOINT pt;
    if (!pPC->getInventory()->getEmptySlot(1, 2, pt)) {
        // There is not enough room in the inventory.
        gcUsePowerPointResult.setErrorCode(GCUsePowerPointResult::NOT_ENOUGH_INVENTORY_SPACE);

        pGamePlayer->sendPacket(&gcUsePowerPointResult);

        return;
    }

    // Create the item at random
    int ratio = rand() % 100 + 1;

    GCUsePowerPointResult::ITEM_CODE itemCode;
    Item::ItemClass itemClass;
    ItemType_t itemType;
    string itemName;

    int sum = 0;
    bool bFind = false;
    for (int i = 0; i < 7; ++i) {
        sum += PowerPointItemTemplate[i].Ratio;
        if (sum >= ratio) {
            bFind = true;
            itemCode = PowerPointItemTemplate[i].ItemCode;
            itemClass = PowerPointItemTemplate[i].ItemClass;
            itemType = PowerPointItemTemplate[i].ItemType;
            itemName = PowerPointItemTemplate[i].ItemName;
            break;
        }
    }

    if (bFind) {
        // Create and add the item
        list<OptionType_t> nullOption;
        Item* pItem = de::gameContext().itemFactories().createItem(itemClass, itemType, nullOption);

        pPC->getZone()->registerObject(pItem);

        if (pPC->getInventory()->addItem(pItem, pt)) {
            // Create the item in the DB
            pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

            // Leave an ItemTraceLog.
            remainTraceLog(pItem, "Mofus", pPC->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);

            // Report the result to the client
            GCCreateItem gcCreateItem;
            gcCreateItem.setObjectID(pItem->getObjectID());
            gcCreateItem.setItemClass(pItem->getItemClass());
            gcCreateItem.setItemType(pItem->getItemType());
            gcCreateItem.setOptionType(pItem->getOptionTypeList());
            gcCreateItem.setDurability(pItem->getDurability());
            gcCreateItem.setItemNum(pItem->getNum());
            gcCreateItem.setInvenX(pt.x);
            gcCreateItem.setInvenY(pt.y);

            pPlayer->sendPacket(&gcCreateItem);

            // The power points needed to create the item
            static int RequirePowerPoint = -300;

            // Save the power points
            pPC->setPowerPoint(savePowerPoint(pPC->getName(), RequirePowerPoint));

            // Report the result to the client
            gcUsePowerPointResult.setErrorCode(GCUsePowerPointResult::NO_ERROR);
            gcUsePowerPointResult.setItemCode(itemCode);
            gcUsePowerPointResult.setPowerPoint(pPC->getPowerPoint());

            pGamePlayer->sendPacket(&gcUsePowerPointResult);

            cout << "--------------------------------------------------" << endl;
            cout << "CREATE ITEM (name:" << pPC->getName() << ",usepowerpoint:" << RequirePowerPoint
                 << ",createditem:" << itemName << ")" << endl;
            cout << "--------------------------------------------------" << endl;

            filelog(MOFUS_LOG_FILE, "CREATE ITEM (name:%s,usepowerpoint:%d,createditem:%s", pPC->getName().c_str(),
                    RequirePowerPoint, itemName.c_str());
        } else {
            cout << "Failed to add to the inventory" << endl;
        }
    } else {
        cout << "Random roll failed ratio : " << ratio << endl;
    }

#endif

#endif

    __END_DEBUG_EX
}
