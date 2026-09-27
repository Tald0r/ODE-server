//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemCommands.cpp
// Description : The GM item creation command.
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include <list>

#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "GCCreateItem.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemFactoryManager.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "StringPool.h"
#include "UniqueItemManager.h"
#include "VSDateTime.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "gm/GMCommands.h"
#include "item/PetItem.h"
#include "repository/ItemRepository.h"

namespace de::gm {
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void opcreate(GamePlayer* pGamePlayer, string msg, int i) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

        if (pGamePlayer == NULL) return;

    Creature* pCreature = pGamePlayer->getCreature();

    size_t j = msg.find_first_of(' ', i + 1);    // class
    size_t k = msg.find_first_of(' ', j + 1);    // type
    size_t l = msg.find_first_of(' ', k + 1);    // option
    size_t lNum = msg.find_first_of('[', k + 1); // num
    size_t rNum = msg.find_first_of(']', k + 1);
    size_t lTime = msg.find_first_of('(', k + 1); // num
    size_t rTime = msg.find_first_of(')', k + 1);
    size_t lGrade = msg.find_first_of('{', k + 1); // num
    size_t rGrade = msg.find_first_of('}', k + 1);

    string optional;

    if (lNum < rNum) {
        optional = msg.substr(lNum + 1, rNum - lNum - 1).c_str();
    }

    ItemType_t ItemType = 255;
    ItemType_t OptionType = 255;

    string ItemClassName = trim(msg.substr(j + 1, k - j - 1));
    Item::ItemClass ItemClass = de::gameContext().itemFactories().getItemClassByName(ItemClassName);

    // ItemClass being MAX means it was not found by that name.
    // In that case it has to be checked whether the item class was given directly as a number.
    if (ItemClass == Item::ITEM_CLASS_MAX) {
        int temp = atoi(ItemClassName.c_str());
        if (temp < 0 || temp >= Item::ITEM_CLASS_MAX) {
            return;
        } else {
            ItemClass = (Item::ItemClass)(temp);
        }
    }

    if (ItemClass == Item::ITEM_CLASS_CORPSE
        //		|| ItemClass == Item::ITEM_CLASS_KEY
        || ItemClass == Item::ITEM_CLASS_MOTORCYCLE || (isRelicItem(ItemClass) && optional != "force")) {
        // Creating an item is blocked
        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(de::gameContext().strings().getString(STRID_CANNOT_CREATE_ITEM));

        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    ItemType = atoi(msg.substr(k + 1, l - k - 1).c_str());

    list<OptionType_t> optionTypes;


    size_t pos = l, previous = l;
    while (previous < string::npos && pos < string::npos) {
        pos = msg.find_first_of(' ', previous + 1);
        string optionString = trim(msg.substr(previous + 1, pos - previous - 1));


        if (optionString.size() == 0)
            break;

        OptionInfo* pOptionInfo = NULL;

        pOptionInfo = de::gameContext().optionInfos().getOptionInfo(optionString);

        if (pOptionInfo == NULL) {
            pOptionInfo = de::gameContext().optionInfos().getOptionInfo(atoi(optionString.c_str()));

            if (pOptionInfo == NULL)
                break;
        }


        if (pOptionInfo != NULL) {
            OptionType = pOptionInfo->getType();

            if (OptionType != 0) {
                optionTypes.push_back(OptionType);
            }
        }

        previous = pos;
    }


    // Return when it is not an item that can really be created.
    if (!de::gameContext().itemInfos().isPossibleItem(ItemClass, ItemType, optionTypes)) {
        StringStream msg;
        msg << de::gameContext().strings().getString(STRID_CANNOT_CREATE_ITEM_2) << ItemClass2ShortString[ItemClass]
            << ", " << (int)ItemType << ", " << getOptionTypeToString(optionTypes);

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg.toString().c_str());

        pGamePlayer->sendPacket(&gcSystemMessage);

        return;
    }

    //  Should a unique item made by create have its count limited too?
    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(ItemClass, ItemType);
    Assert(pItemInfo != NULL);

    // For a unique item

    Item* pItem = de::gameContext().itemFactories().createItem((Item::ItemClass)ItemClass, ItemType, optionTypes);
    pItem->setCreateType(Item::CREATE_TYPE_CREATE);
    pItem->setNum(1);

    if (ItemClass == Item::ITEM_CLASS_PET_ITEM) {
        PetInfo* pPetInfo = new PetInfo;
        pPetInfo->setPetType(ItemType);
        pPetInfo->setPetCreatureType(432);
        pPetInfo->setPetLevel(1);
        pPetInfo->setPetExp(0);
        pPetInfo->setPetHP(1000);
        pPetInfo->setPetAttr(0xff);
        pPetInfo->setPetOption(0);
        pPetInfo->setFoodType(0);
        pPetInfo->setFeedTime(VSDateTime::currentDateTime());

        PetItem* pPetItem = dynamic_cast<PetItem*>(pItem);
        if (pPetItem != NULL) {
            pPetItem->setPetInfo(pPetInfo);
            pPetInfo->setPetItem(pPetItem);
        } else {
            cout << "-_-;;;;" << endl;
        }
    }


    if (isStackable(pItem->getItemClass()) && lNum < rNum) {
        int itemNum = atoi(msg.substr(lNum + 1, rNum - lNum - 1).c_str());

        itemNum = max(1, min(itemNum, ItemMaxStack[pItem->getItemClass()]));

        pItem->setNum(itemNum);
    }

    Assert(pItem != NULL);

    Zone* pZone = pCreature->getZone();

    ObjectRegistry& objectregistery = pZone->getObjectRegistry();
    objectregistery.registerObject(pItem);

    GCCreateItem gcCreateItem;

    if (pCreature->isPC()) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

        Inventory* pInventory = pPC->getInventory();

        Assert(pInventory != NULL);

        TPOINT p;

        if (pInventory->getEmptySlot(pItem, p)) {
            pInventory->addItem(p.x, p.y, pItem);
            if (lGrade < rGrade) {
                int grade = atoi(msg.substr(lGrade + 1, rGrade - lGrade - 1).c_str());
                pItem->setGrade(grade);
            }

            pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, p.x, p.y);
            pItem->whenPCTake(pPC);

            if (lTime < rTime) {
                int time = atoi(msg.substr(lTime + 1, rTime - lTime - 1).c_str());
                if (time > 0) {
                    pPC->addTimeLimitItem(pItem, time);
                    pPC->sendTimeLimitItemInfo();
                }
            } else {
                pPC->addTimeLimitItem(pItem, 2592000);
                pPC->sendTimeLimitItemInfo();
            }

            makeGCCreateItem(&gcCreateItem, pItem, p.x, p.y);
            pGamePlayer->sendPacket(&gcCreateItem);
        } else {
            SAFE_DELETE(pItem);
        }
    }

    if (pItem != NULL) {
        if (isRelicItem(pItem)) {
            addRelicEffect(pCreature, pItem);
        }

        // For a unique item, mark it unique.
        if (pItemInfo->isUnique()) {
            pItem->setUnique();
            filelog("uniqueItem.txt", "[OpCreate] %s %s", pCreature->getName().c_str(), pItem->toString().c_str());
        }


        // Leave a log.
        defaultItemRepository().insertOpCreateLog(pCreature->getName(), VSDateTime::currentDateTime().toString(),
                                                  pItem->toString());
        if (pItem != NULL && pItem->isTraceItem()) {
            remainTraceLog(pItem, "GOD", pCreature->getName(), ITEM_LOG_CREATE, DETAIL_COMMAND);
        }
    }

    __END_DEBUG_EX __END_CATCH
}
} // namespace de::gm
