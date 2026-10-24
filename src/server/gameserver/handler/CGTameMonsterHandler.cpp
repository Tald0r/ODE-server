//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTameMonsterHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGTameMonster.h"
#include "GameContext.h"
#include "ItemInfoManager.h"

#ifdef __GAME_SERVER__

#include <list>

#include "CreatureUtil.h"
#include "GCCannotUse.h"
#include "GCCreateItem.h"
#include "GCDeleteInventoryItem.h"
#include "GQuestManager.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "Monster.h"
#include "PacketUtil.h"
#include "PetFood.h"
#include "PetInfo.h"
#include "PetItem.h"
#include "PetTypeInfo.h"
#include "PlayerCreature.h"
#include "VSDateTime.h"
#include "VariableManager.h"
#include "Zone.h"

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGTameMonsterHandler::execute(CGTameMonster* pPacket, Player* pPlayer)

{
#ifdef __GAME_SERVER__

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    Assert(pPC != NULL);

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    Monster* pMonster = dynamic_cast<Monster*>(pZone->getCreature(pPacket->getObjectID()));
    if (pMonster == NULL)
        return;

    Item* pItem = pPC->getExtraInventorySlotItem();
    if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_PET_FOOD || pItem->getNum() != 1)
        return;

    PetFoodInfo* pPetFoodInfo = dynamic_cast<PetFoodInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_PET_FOOD, pItem->getItemType()));
    Assert(pPetFoodInfo != NULL);

    Inventory* pInventory = pPC->getInventory();
    if (pInventory == NULL)
        return;

    ItemType_t petType = PET_COMMON;
    PetItem* pPetItem = dynamic_cast<PetItem*>(
        de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_PET_ITEM, petType, list<OptionType_t>()));
    Assert(pPetItem != NULL);

    _TPOINT pt;
    if (!pInventory->getEmptySlot(pPetItem, pt)) {
        GCCannotUse gcCannotUse;
        pGamePlayer->sendPacket(&gcCannotUse);
        SAFE_DELETE(pPetItem);
        return;
    }

    pPC->deleteItemFromExtraInventorySlot();
    GCDeleteInventoryItem gcDI;
    gcDI.setObjectID(pItem->getObjectID());
    pGamePlayer->sendPacket(&gcDI);

    pItem->destroy();
    SAFE_DELETE(pItem);

    /*
     * Something has to be checked here.
     */

    int ratio = rand() % 100;

    if (de::gameContext().variables().getVariable(PET_DAY_EVENT) != 0) {
        ratio = 100;
    }

    // Only a common pet can be tamed.
    PetTypeInfo* pPetTypeInfo = PetTypeInfoManager::getInstance()->getPetTypeInfo(petType);
    if (pPetTypeInfo == NULL || pPetTypeInfo->getOriginalMonsterType() != pMonster->getMonsterType() ||
        ratio >= pPetFoodInfo->getTameRatio()) {
        SAFE_DELETE(pPetItem);
        pMonster->addEnemy(pPC);
        return;
    }

    pZone->deleteCreature(pMonster, pMonster->getX(), pMonster->getY());
    SAFE_DELETE(pMonster);

    /*
     * The pet's information has to be fetched here.
     */

    PetInfo* pPetInfo = new PetInfo;

    pPetInfo->setPetType(petType);
    pPetInfo->setPetLevel(0);
    pPetInfo->setPetCreatureType(pPetTypeInfo->getPetCreatureType(0));
    pPetInfo->setPetAttr(0xff);
    pPetInfo->setPetExp(0);
    pPetInfo->setPetAttrLevel(0);
    pPetInfo->setFoodType(pPetFoodInfo->getItemType());
    pPetInfo->setGamble(0);
    pPetInfo->setCutHead(0);
    pPetInfo->setPetHP(pPetFoodInfo->getPetHP());
    pPetInfo->setFeedTime(VSDateTime::currentDateTime());

    // Two-way link
    pPetItem->setPetInfo(pPetInfo);
    pPetInfo->setPetItem(pPetItem);

    pZone->registerObject(pPetItem);
    pInventory->addItem(pPetItem, pt);
    Assert(pt.x != -1);

    pPetItem->create(pPC->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

    // Leave a TraceLog.
    remainTraceLog(pPetItem, "GOD", pPC->getName(), ITEM_LOG_CREATE, DETAIL_PICKUP);

    GCCreateItem gcCreateItem;
    makeGCCreateItem(&gcCreateItem, pPetItem, pt.x, pt.y);

    pGamePlayer->sendPacket(&gcCreateItem);


    pPC->getGQuestManager()->tamePet(pPetInfo);
    addOlympicStat(pPC, 9);

#endif
}
