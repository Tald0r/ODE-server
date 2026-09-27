////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionTradeEventItem.cpp
// Description :
////////////////////////////////////////////////////////////////////////////////
#include "ActionTradeEventItem.h"

#include <stdio.h>

#include <list>

#include "DB.h"
#include "GCCreateItem.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "PlayerCreature.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Thread.h"
#include "Vampire.h"
#include "Zone.h"
#include "item/Key.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionTradeEventItem::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY

    try {
        // Read the amount of money to receive.
        m_GoalAmount = (Gold_t)propertyBuffer.getPropertyInt("Amount");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionTradeEventItem::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    StringPool& strings = context().strings();

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    Inventory* pInventory = pPC->getInventory();
    XMAS_STAR star;

    // Send GCNPCResponse to the client first.
    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);
    char message[100];

    // Check whether this player's inventory holds enough soccer balls.


    context().prices().getBallPrice(m_GoalAmount, star);

    if (!pInventory->hasEnoughStar(star)) {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_REQUIRE_MORE_SOCCER_BALL));
        pPlayer->sendPacket(&gcSystemMessage);

        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&response);

        return;
    }

    // If this player's inventory has enough room, put the items in.
    // First create the two items.
    Zone* pZone = pPC->getZone();
    ObjectRegistry& OR = pZone->getObjectRegistry();
    ItemFactoryManager& itemFactories = context().itemFactories();

    TPOINT pt, pt2;
    Item* pItem1;
    Item* pItem2;

    list<OptionType_t> option1;
    option1.push_back(1);
    list<OptionType_t> option2;
    option2.push_back(2);
    list<OptionType_t> option3;
    option3.push_back(3);
    list<OptionType_t> option4;
    option4.push_back(4);
    list<OptionType_t> option5;
    option5.push_back(5);

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        if (pSlayer->getSex() == 1) // male
        {
            switch (m_GoalAmount) {
            case 100:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 0, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 0, option4);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_1));
                break;
            case 125:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 2, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 2, option5);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_2));
                break;
            case 175:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 4, option1);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 4, option1);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_3));
                break;
            case 225:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 4, option2);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 4, option2);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_4));
                break;
            case 275:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 4, option3);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 4, option3);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_5));
                break;
            default:
                return;
            }
        } else if (pSlayer->getSex() == 0) {
            switch (m_GoalAmount) {
            case 100:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 1, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 1, option4);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_6));
                break;
            case 125:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 3, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 3, option5);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_7));
                break;
            case 175:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 5, option1);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 5, option1);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_8));
                break;
            case 225:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 5, option2);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 5, option2);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_9));
                break;
            case 275:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_COAT, 5, option3);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_TROUSER, 5, option3);
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_10));
                break;
            default:
                return;
            }
        } else
            return;
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
        if (pVampire->getSex() == 0) {
            switch (m_GoalAmount) {
            case 100:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 1, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 2, option4);
                // message << "STR+4 bodysuit and STR+4 Cuspid necklace granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_11));
                break;
            case 125:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 3, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 3, option4);
                // message << "STR+4 jumpsuit and STR+4 Jet necklace granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_12));
                break;
            case 175:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 3, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 4, option3);
                // message << "STR+5 jumpsuit and STR+4 Idol pendant granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_13));
                break;
            case 225:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 3, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 5, option3);
                // message << "STR+5 jumpsuit and STR+4 Anti-Cross pendant
                // granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_14));
                break;
            case 275:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 3, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 6, option3);
                // message << "STR+5 jumpsuit and STR+4 Death-Star pendant granted.
                // Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_15));
                break;
            default:
                return;
            }
        } else if (pVampire->getSex() == 1) {
            switch (m_GoalAmount) {
            case 100:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 0, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 2, option4);
                // message << "STR+4 swing coat and STR+4 Cuspid necklace granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_16));
                break;
            case 125:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 2, option4);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 3, option4);
                // message << "STR+4 frock coat and STR+4 Jet necklace granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_17));
                break;
            case 175:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 2, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 4, option3);
                // message << "STR+5 frock coat and STR+4 Idol pendant granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_18));
                break;
            case 225:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 2, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 5, option3);
                // message << "STR+5 frock coat and STR+4 Anti-Cross pendant
                // granted. Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_19));
                break;
            case 275:
                pItem1 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_COAT, 2, option5);
                pItem2 = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 6, option3);
                // message << "STR+5 frock coat and STR+4 Death-Star pendant granted.
                // Go Korea team!";
                snprintf(message, sizeof(message), strings.c_str(STRID_KOREA_FIGHTING_20));
                break;
            default:
                return;
            }
        } else
            return;
    } else
        return;

    OR.registerObject(pItem1);
    OR.registerObject(pItem2);

    // If the inventory has room, put them in.
    // Two items have to go in, so one is added and then the other, and
    // once both are in the number of soccer balls is reduced.
    // If the second one does not fit, the first item is deleted.
    if (pInventory->addItem(pItem1, pt)) {
        if (pInventory->addItem(pItem2, pt2)) {
            pItem1->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);
            pItem2->create(pPC->getName(), STORAGE_INVENTORY, 0, pt2.x, pt2.y);

            // Leave an ItemTraceLog.
            if (pItem1 != NULL && pItem1->isTraceItem()) {
                remainTraceLog(pItem1, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
            }

            // Leave an ItemTraceLog.
            if (pItem2 != NULL && pItem2->isTraceItem()) {
                remainTraceLog(pItem2, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
            }
            GCCreateItem gcCreateItem;
            gcCreateItem.setObjectID(pItem1->getObjectID());
            gcCreateItem.setItemClass(pItem1->getItemClass());
            gcCreateItem.setItemType(pItem1->getItemType());
            gcCreateItem.setOptionType(pItem1->getOptionTypeList());
            gcCreateItem.setDurability(pItem1->getDurability());
            gcCreateItem.setItemNum(pItem1->getNum());
            gcCreateItem.setInvenX(pt.x);
            gcCreateItem.setInvenY(pt.y);

            pPlayer->sendPacket(&gcCreateItem);


            gcCreateItem.setObjectID(pItem2->getObjectID());
            gcCreateItem.setItemClass(pItem2->getItemClass());
            gcCreateItem.setItemType(pItem2->getItemType());
            gcCreateItem.setOptionType(pItem2->getOptionTypeList());
            gcCreateItem.setDurability(pItem2->getDurability());
            gcCreateItem.setItemNum(pItem2->getNum());
            gcCreateItem.setInvenX(pt2.x);
            gcCreateItem.setInvenY(pt2.y);

            pPlayer->sendPacket(&gcCreateItem);

            // Reduce the soccer balls.
            pInventory->decreaseStar(star);
            cout << "Reduced the player's soccer balls by " << star.amount << "." << endl;

            GCNPCResponse gcNPCResponse;
            gcNPCResponse.setCode(NPC_RESPONSE_DECREASE_BALL);
            gcNPCResponse.setParameter(star.amount);
            pPlayer->sendPacket(&gcNPCResponse);

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(message);
            pPlayer->sendPacket(&gcSystemMessage);

        } else {
            pInventory->deleteItem(pt.x, pt.y);
            SAFE_DELETE(pItem1);
            SAFE_DELETE(pItem2);


            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_NOT_ENOUGH_INVENTORY_SPACE));
            pPlayer->sendPacket(&gcSystemMessage);
        }
    } else {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_NOT_ENOUGH_INVENTORY_SPACE));
        pPlayer->sendPacket(&gcSystemMessage);
    }

    GCNPCResponse response;
    response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
    pPlayer->sendPacket(&response);

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionTradeEventItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionTradeEventItem(" << ")";
    return msg.toString();

    __END_CATCH
}
