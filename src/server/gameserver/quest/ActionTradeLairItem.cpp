////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionTradeLairItem.cpp
// Description :
//               Exchanges the item dropped when a lair master is killed
//               with an NPC.
//               The created item is decided at random. (Using Gamble well
//               would probably make this easy.)
//
//
// History.
//
//  Date        Writer         Description
// ---------- ----------- ------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////

#include "ActionTradeLairItem.h"

#include <list>

#include "DB.h"
#include "GCCreateItem.h"
#include "GCDeleteInventoryItem.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemGradeManager.h"
#include "ItemInfoManager.h"
#include "ItemMineInfo.h"
#include "ItemUtil.h"
#include "MonsterInfo.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "QuestItem.h"
#include "Slayer.h"
#include "StringPool.h"
#include "SystemAvailabilitiesManager.h"
#include "Thread.h"
#include "Treasure.h"
#include "Vampire.h"
#include "Zone.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionTradeLairItem::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY

    try {
        // Read the amount of money to receive.
        m_Type = (ItemType_t)propertyBuffer.getPropertyInt("Type");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionTradeLairItem::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    StringPool& strings = context().strings();

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    if (m_Type <= 5) {
        SYSTEM_RETURN_IF_NOT(SYSTEM_MASTER_LAIR);
    }

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    Player* pPlayer = pPC->getPlayer();
    Assert(pPlayer != NULL);

    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);
    StringStream message;


    Inventory* pInventory = pPC->getInventory();

    // First check whether the player holds the item.
    Item* pMasterItem = NULL;
    MonsterType_t MonsterType = 0;

    ItemMineInfoManager& itemMineInfos = context().itemMineInfos();
    Item* pItem1 = NULL;

    // A different item is checked depending on the option.
    // Conan: takes Bathory's bijou/pendant, falling back to Tepes' and then
    //   to Gilles de Rais'.
    // Vrykolakas: takes only Tepes' bijou/pendant.
    // Caim: takes only Bathory's bijou/pendant.
    // Gilles de Rais: takes only its own bijou/pendant.

    if (m_Type == 0) // Conan, bijou
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 0);
        MonsterType = BATORI_TYPE;

        if (pMasterItem == NULL) {
            pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 2);
            MonsterType = TEPEZ_TYPE;
            if (pMasterItem == NULL) {
                pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 8); // Gilles de Rais bijou
                MonsterType = GDR_TYPE;
            }
        }
    } else if (m_Type == 1) // Conan, pendant
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 1);
        MonsterType = BATORI_TYPE;

        if (pMasterItem == NULL) {
            pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 3);
            MonsterType = TEPEZ_TYPE;
            if (pMasterItem == NULL) {
                pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 9); // Gilles de Rais pendant
                MonsterType = GDR_TYPE;
            }
        }
    } else if (m_Type == 2) // Vrykolakas, bijou
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 2);
        // Bijou
        MonsterType = TEPEZ_TYPE;
    } else if (m_Type == 3) // Vrykolakas, pendant
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 3);
        // Pendant
        MonsterType = TEPEZ_TYPE;
    } else if (m_Type == 4) // Caim, bijou
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 0);
        // Bijou
        MonsterType = BATORI_TYPE;
    } else if (m_Type == 5) // Caim, pendant
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 1);
        // Pendant
        MonsterType = BATORI_TYPE;
    } else if (m_Type == 10) // Gilles de Rais, bijou
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 8);
        MonsterType = GDR_TYPE;
    } else if (m_Type == 11) // Gilles de Rais, pendant
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 9);
        MonsterType = GDR_TYPE;
    } else if (m_Type == 6) // Gemstone
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 4);
        MonsterType = BATORI_TYPE;
    } else if (m_Type == 7) // Full moon
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 5);
        ItemMineInfo* pItemMineInfo = NULL;

        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            Assert(pSlayer != NULL);

            Attr_t totalAttr = pSlayer->getTotalAttr(ATTR_BASIC);

            if (totalAttr <= 130) // Hardcoded
                pItemMineInfo = itemMineInfos.getItemMineInfo(0);
            else if (totalAttr <= 210)
                pItemMineInfo = itemMineInfos.getItemMineInfo(1);
            else if (totalAttr <= 270)
                pItemMineInfo = itemMineInfos.getItemMineInfo(2);
            else if (totalAttr <= 300)
                pItemMineInfo = itemMineInfos.getItemMineInfo(3);
            else
                pItemMineInfo = itemMineInfos.getItemMineInfo(4);
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            Assert(pVampire != NULL);

            Level_t level = pVampire->getLevel();

            if (level <= 20)
                pItemMineInfo = itemMineInfos.getItemMineInfo(5);
            else if (level <= 40)
                pItemMineInfo = itemMineInfos.getItemMineInfo(6);
            else if (level <= 60)
                pItemMineInfo = itemMineInfos.getItemMineInfo(7);
            else if (level <= 90)
                pItemMineInfo = itemMineInfos.getItemMineInfo(8);
            else
                pItemMineInfo = itemMineInfos.getItemMineInfo(9);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            Assert(pOusters != NULL);

            Level_t level = pOusters->getLevel();

            if (level <= 20)
                pItemMineInfo = itemMineInfos.getItemMineInfo(10);
            else if (level <= 40)
                pItemMineInfo = itemMineInfos.getItemMineInfo(11);
            else if (level <= 60)
                pItemMineInfo = itemMineInfos.getItemMineInfo(12);
            else if (level <= 90)
                pItemMineInfo = itemMineInfos.getItemMineInfo(13);
            else
                pItemMineInfo = itemMineInfos.getItemMineInfo(14);
        } else
            Assert(false);

        // A mine table with no row for the band leaves the trade with no
        // item, which the no-item handling below answers.
        if (pItemMineInfo != NULL)
            pItem1 = pItemMineInfo->getItem();

        if (pItem1 != NULL)
            setItemGender(pItem1, (pPC->getSex() == FEMALE) ? GENDER_FEMALE : GENDER_MALE);
    } else if (m_Type == 8) // Dark moon
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 6);

        if (pPC->isSlayer()) {
            pItem1 = itemMineInfos.getRandomItem(15, 32);
        } else if (pPC->isVampire()) {
            pItem1 = itemMineInfos.getRandomItem(33, 45);
        } else if (pPC->isOusters()) {
            pItem1 = itemMineInfos.getRandomItem(46, 61);
        }

        if (pItem1 != NULL)
            setItemGender(pItem1, (pPC->getSex() == FEMALE) ? GENDER_FEMALE : GENDER_MALE);
    } else if (m_Type == 9) // Red lucky pouch
    {
        pMasterItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM, 7);

        if (pPC->isSlayer()) {
            pItem1 = itemMineInfos.getRandomItem(62, 81);
        } else if (pPC->isVampire()) {
            pItem1 = itemMineInfos.getRandomItem(82, 96);
        } else if (pPC->isOusters()) {
            pItem1 = itemMineInfos.getRandomItem(97, 112);
        }
    } else {
        // The NPC's trade property is wrong, so the player is told that the
        // operations team can sort it out right away.
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_NPC_ERROR));
        pPlayer->sendPacket(&gcSystemMessage);

        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&response);
        return;
    }

    if (pMasterItem == NULL) {
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_NO_LAIR_ITEM));
        pPlayer->sendPacket(&gcSystemMessage);

        GCNPCResponse response;
        response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
        pPlayer->sendPacket(&response);

        return;
    }

    Zone* pZone = pPC->getZone();

    if (pItem1 == NULL) {
        QuestItemInfo* pItemInfo = dynamic_cast<QuestItemInfo*>(
            context().itemInfos().getItemInfo(Item::ITEM_CLASS_QUEST_ITEM, pMasterItem->getItemType()));
        Assert(pItemInfo != NULL);

        ///////////////////////////////////////////////////////////////////////////////
        // The reward is rolled from the monster's treasure list: every treasure
        // in the list is rolled in turn and the last one that yields an item wins.
        //////////////////////////////////////////////////////////////////////////////
        const MonsterInfo* pMonsterInfo = context().monsterInfos().getMonsterInfo(MonsterType);
        TreasureList* pTreasureList = NULL;

        // The item given differs by race as well.
        if (pCreature2->isSlayer())
            pTreasureList = pMonsterInfo->getSlayerTreasureList();
        else if (pCreature2->isVampire())
            pTreasureList = pMonsterInfo->getVampireTreasureList();
        else if (pCreature2->isOusters())
            pTreasureList = pMonsterInfo->getOustersTreasureList();

        const list<Treasure*>& treasures = pTreasureList->getTreasures();

        list<Treasure*>::const_iterator itr = treasures.begin();

        ITEM_TEMPLATE it;

        for (; itr != treasures.end(); itr++) {
            Treasure* pTreasure = (*itr);

            it.ItemClass = Item::ITEM_CLASS_MAX;
            it.ItemType = 0;

            // Chance of a second option, which differs per quest item.
            it.NextOptionRatio = pItemInfo->getBonusRatio();


            if (pTreasure->getRandomItem(&it)) {
                // The last roll wins, so an item an earlier one produced is
                // dropped here rather than left with no owner.
                SAFE_DELETE(pItem1);

                pItem1 = context().itemFactories().createItem(it.ItemClass, it.ItemType, it.OptionType);
                Assert(pItem1 != NULL);
            }
        }

        if (pItem1 == NULL) {
            StringStream msg;
            msg << "ActionTradeLairItem: " << (int)it.ItemClass << ", " << (int)it.ItemType << ", "
                << (int)it.bCreateOption << ", " << getOptionTypeToString(it.OptionType);

            filelog("tradeLairItemBUG.txt", "%s", msg.toString().c_str());

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_ITEM_CREATE_ERROR));
            pPlayer->sendPacket(&gcSystemMessage);

            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
            pPlayer->sendPacket(&response);

            return;
        }
    }

    TPOINT pt;

    pItem1->setGrade(min(7, ItemGradeManager::Instance().getRandomBeadGrade()));
    pZone->registerObject(pItem1);

    // If the inventory has room, put them in.
    if (pInventory->addItem(pItem1, pt)) {
        pItem1->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

        GCCreateItem gcCreateItem;

        makeGCCreateItem(&gcCreateItem, pItem1, pt.x, pt.y);

        pPlayer->sendPacket(&gcCreateItem);

        // Leave an ItemTraceLog.
        if (pItem1 != NULL && pItem1->isTraceItem()) {
            remainTraceLog(pItem1, pCreature1->getName(), pCreature2->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
        }

        // Remove the existing item.
        GCDeleteInventoryItem gcDeleteInventoryItem;
        gcDeleteInventoryItem.setObjectID(pMasterItem->getObjectID());
        pPlayer->sendPacket(&gcDeleteInventoryItem);

        // Remove it on the server side.
        pInventory->deleteItem(pMasterItem->getObjectID()); // Deleting by coordinates would be nicer here.
        // Leave an ItemTraceLog.
        if (pMasterItem != NULL && pMasterItem->isTraceItem()) {
            remainTraceLog(pMasterItem, pCreature2->getName(), pCreature1->getName(), ITEM_LOG_DELETE, DETAIL_EVENTNPC);
        }
        pMasterItem->destroy();
        SAFE_DELETE(pMasterItem);

        // Print a success message to the user.
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(strings.getString(STRID_TRADE_SUCCESS));
        pPlayer->sendPacket(&gcSystemMessage);
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
string ActionTradeLairItem::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionTradeLairItem(" << ")";
    return msg.toString();

    __END_CATCH
}
