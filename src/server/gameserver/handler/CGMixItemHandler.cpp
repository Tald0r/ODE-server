//////////////////////////////////////////////////////////////////////////////
// Filename    : CGMixItemHandler.cpp
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGMixItem.h"

#ifdef __GAME_SERVER__
#include <cstdio>

#include "GCAddItemToItemVerify.h"
#include "GCCannotUse.h"
#include "GCUseOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "MixingItem.h"
#include "OptionInfo.h"
#include "PlayerCreature.h"
void sendCannotUse(CGMixItem* pPacket, Player* pPlayer);
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGMixItemHandler::execute(CGMixItem* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();

    Assert(pInventory != NULL);
    Assert(pZone != NULL);

    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();


    // An area beyond the inventory coordinates is not allowed.
    if (InvenX >= pInventory->getWidth() || InvenY >= pInventory->getHeight()) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    Item* pItem = pPC->findItemOID(pPacket->getObjectID());

    if (pItem == NULL) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    // Get the Object of the item in the inventory.


    // It has to be a mixing forge
    if (pItem->getItemClass() != Item::ITEM_CLASS_MIXING_ITEM) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    MixingItemInfo* pInfo = dynamic_cast<MixingItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    Assert(pInfo != NULL);

    switch (pInfo->getType()) {
    case MixingItemInfo::TYPE_MIX: {
        Item* pTempItem = pInventory->getItem(InvenX, InvenY);
        if (pTempItem == NULL || pTempItem->getObjectID() != pItem->getObjectID()) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        executeMix(pPacket, pPlayer, pItem);
    } break;
    case MixingItemInfo::TYPE_DETACH:
        executeDetach(pPacket, pPlayer, pItem);
        break;

    case MixingItemInfo::TYPE_CLEAR:
        executeClearOption(pPacket, pPlayer, pItem);
        break;

    default:
        sendCannotUse(pPacket, pPlayer);
        break;
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

#ifdef __GAME_SERVER__
void CGMixItemHandler::executeMix(CGMixItem* pPacket, Player* pPlayer, Item* pItem) {
    __BEGIN_TRY

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    OptionInfoManager& optionInfos = de::gameContext().optionInfos();
    Zone* pZone = pPC->getZone();

    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();

    Assert(pInventory != NULL);
    Assert(pZone != NULL);

    Item* pTargetItem1 = NULL;
    Item* pTargetItem2 = NULL;

    CoordInven_t dummy;

    pTargetItem1 = pInventory->findItemOID(pPacket->getTargetObjectID(0), dummy, dummy);
    pTargetItem2 = pInventory->findItemOID(pPacket->getTargetObjectID(1), dummy, dummy);

    if (pTargetItem1 == NULL || pTargetItem2 == NULL) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    ItemInfo* pInfo1 =
        de::gameContext().itemInfos().getItemInfo(pTargetItem1->getItemClass(), pTargetItem1->getItemType());
    int GradeDiff = pTargetItem1->getGrade() - pTargetItem2->getGrade();
    Grade_t TargetGrade =
        (pTargetItem1->getGrade() > pTargetItem2->getGrade()) ? pTargetItem1->getGrade() : pTargetItem2->getGrade();

    if (pTargetItem1->getItemClass() != pTargetItem2->getItemClass() ||
        pTargetItem1->getItemType() != pTargetItem2->getItemType() || pTargetItem1->getOptionTypeSize() != 1 ||
        pTargetItem2->getOptionTypeSize() != 1 || pTargetItem1->isTimeLimitItem() || pTargetItem2->isTimeLimitItem() ||
        pInfo1->isUnique() || GradeDiff < -2 || GradeDiff > 2) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    MixingItemInfo* pInfo = dynamic_cast<MixingItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    Assert(pInfo != NULL);

    MixingItemInfo::Target target = pInfo->getTarget();

    switch (target) {
    case MixingItemInfo::MIXING_WEAPON: {
        if (isSlayerWeapon(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireWeapon(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersWeapon(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemClass() == Item::ITEM_CLASS_OUSTERS_WRISTLET) {
                if (pInfo1->getItemLevel() > pInfo->getOustersLevel()) {
                    sendCannotUse(pPacket, pPlayer);
                    return;
                }
            } else if (pTargetItem1->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    case MixingItemInfo::MIXING_ARMOR: {
        if (isSlayerArmor(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireArmor(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersArmor(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    case MixingItemInfo::MIXING_ACCESSORY: {
        if (isSlayerAccessory(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireAccessory(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersAccessory(pTargetItem1->getItemClass())) {
            if (pTargetItem1->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE &&
                pTargetItem1->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    default: {
        sendCannotUse(pPacket, pPlayer);
        return;
    }
    }

    GCAddItemToItemVerify gcVerify;

    const list<OptionType_t>& oList1 = pTargetItem1->getOptionTypeList();
    Assert(!oList1.empty());
    OptionType_t option1 = oList1.front();
    OptionInfo* pOptionInfo1 = optionInfos.getOptionInfo(option1);

    const list<OptionType_t>& oList2 = pTargetItem2->getOptionTypeList();
    Assert(!oList2.empty());
    OptionType_t option2 = oList2.front();
    OptionInfo* pOptionInfo2 = optionInfos.getOptionInfo(option2);

    if (optionInfos.getOptionClassInfo(pOptionInfo1->getClass())->getOptionGroup() ==
        optionInfos.getOptionClassInfo(pOptionInfo2->getClass())->getOptionGroup()) {
        gcVerify.setCode(ADD_ITEM_TO_ITEM_VERIFY_MIXING_FAILED_SAME_OPTION_GROUP);
        pPlayer->sendPacket(&gcVerify);
        return;
    }

    pTargetItem1->addOptionType(option2);
    // If either pTargetItem1 or pTargetItem2 is traced, what is made is traced too
    pTargetItem1->setTraceItem(pTargetItem1->isTraceItem() || pTargetItem2->isTraceItem());
    pTargetItem1->setGrade(TargetGrade);

    string optionfield = "";
    setOptionTypeToField(pTargetItem1->getOptionTypeList(), optionfield);
    char query[100];
    if (TargetGrade == -1)
        snprintf(query, sizeof(query), "OptionType='%s'", optionfield.c_str());
    else
        snprintf(query, sizeof(query), "OptionType='%s', Grade=%d", optionfield.c_str(), pTargetItem1->getGrade());

    pTargetItem1->tinysave(query);

    snprintf(query, sizeof(query), "%u", pTargetItem1->getItemID());

    pInventory->deleteItem(pTargetItem2->getObjectID());
    if (pTargetItem2->isTraceItem()) {
        remainTraceLog(pTargetItem2, pPC->getName(), query, ITEM_LOG_DELETE, DETAIL_ENCHANT);
    }
    pTargetItem2->destroy();
    SAFE_DELETE(pTargetItem2);

    if (pItem->isTraceItem()) {
        remainTraceLog(pItem, pPC->getName(), "Mixing Forge", ITEM_LOG_DELETE, DETAIL_ENCHANT);
    }

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        pInventory->deleteItem(pItem->getObjectID());
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

    gcVerify.setCode(ADD_ITEM_TO_ITEM_VERIFY_MIXING_OK);
    uint param = 0;

    param = ((((int)option1) << 16) | option2);
    printf("Param:%x\n", param);

    gcVerify.setParameter(param);

    pPlayer->sendPacket(&gcVerify);

    GCUseOK gcUseOK;
    pPlayer->sendPacket(&gcUseOK);

    __END_CATCH
}

void CGMixItemHandler::executeDetach(CGMixItem* pPacket, Player* pPlayer, Item* pItem) {
    __BEGIN_TRY

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();

    Assert(pInventory != NULL);
    Assert(pZone != NULL);

    Item* pTargetItem = NULL;

    CoordInven_t dummy;

    pTargetItem = pInventory->findItemOID(pPacket->getTargetObjectID(0), dummy, dummy);
    uint optionNo = (uint)pPacket->getTargetObjectID(1);

    if (pTargetItem == NULL || optionNo >= 2 || pItem->getNum() != 1) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    ItemInfo* pTargetInfo =
        de::gameContext().itemInfos().getItemInfo(pTargetItem->getItemClass(), pTargetItem->getItemType());

    if (pTargetInfo->isUnique() || pTargetItem->isTimeLimitItem() || pTargetItem->getOptionTypeSize() != 2) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    MixingItemInfo* pInfo = dynamic_cast<MixingItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    Assert(pInfo != NULL);

    MixingItemInfo::Target target = pInfo->getTarget();

    switch (target) {
    case MixingItemInfo::MIXING_WEAPON: {
        if (isSlayerWeapon(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireWeapon(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersWeapon(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    case MixingItemInfo::MIXING_ARMOR: {
        if (isSlayerArmor(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireArmor(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersArmor(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    case MixingItemInfo::MIXING_ACCESSORY: {
        if (isSlayerAccessory(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireAccessory(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersAccessory(pTargetItem->getItemClass())) {
            if (pTargetItem->getItemClass() != Item::ITEM_CLASS_OUSTERS_STONE &&
                pTargetItem->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    default: {
        sendCannotUse(pPacket, pPlayer);
        return;
    }
    }

    list<OptionType_t> oList = pTargetItem->getOptionTypeList();
    Assert(oList.size() == 2);

    // optionNo is either 0 or 1 (checked above).
    if (optionNo == 0)
        oList.pop_front();
    else
        oList.pop_back();
    pTargetItem->setOptionType(oList);

    string optionfield = "";
    setOptionTypeToField(pTargetItem->getOptionTypeList(), optionfield);
    char query[100];
    snprintf(query, sizeof(query), "OptionType='%s'", optionfield.c_str());
    pTargetItem->tinysave(query);

    pTargetItem->setTraceItem(bTraceLog(pTargetItem));

    Assert(pItem == pPC->getExtraInventorySlotItem());
    // The item the option is taken off is on the mouse. If not, never mind
    pPC->deleteItemFromExtraInventorySlot();
    if (pItem->isTraceItem()) {
        remainTraceLog(pItem, pPC->getName(), "Furitas", ITEM_LOG_DELETE, DETAIL_ENCHANT);
    }
    pItem->destroy();
    SAFE_DELETE(pItem);

    GCAddItemToItemVerify gcVerify;
    gcVerify.setCode(ADD_ITEM_TO_ITEM_VERIFY_DETACHING_OK);

    OptionType_t finalOption = oList.front();
    gcVerify.setParameter(finalOption);

    pPlayer->sendPacket(&gcVerify);


    __END_CATCH
}

void CGMixItemHandler::executeClearOption(CGMixItem* pPacket, Player* pPlayer, Item* pItem) {
    __BEGIN_TRY

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Assert(pPC != NULL);

    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();

    Assert(pInventory != NULL);
    Assert(pZone != NULL);

    Item* pTargetItem = NULL;

    CoordInven_t dummy;

    pTargetItem = pInventory->findItemOID(pPacket->getTargetObjectID(0), dummy, dummy);
    uint optionNo = (uint)pPacket->getTargetObjectID(1);

    if (pTargetItem == NULL || optionNo >= 2 || pItem->getNum() != 1) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    Item::ItemClass iClass = pTargetItem->getItemClass();

    ItemInfo* pTargetInfo =
        de::gameContext().itemInfos().getItemInfo(pTargetItem->getItemClass(), pTargetItem->getItemType());

    if (pTargetInfo->isUnique() || pTargetItem->isTimeLimitItem() || pTargetItem->getOptionTypeSize() != 1) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    MixingItemInfo* pInfo = dynamic_cast<MixingItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    Assert(pInfo != NULL);

    MixingItemInfo::Target target = pInfo->getTarget();

    switch (target) {
    case MixingItemInfo::MIXING_ALL: {
        if (isSlayerWeapon(iClass) || isSlayerArmor(iClass) || isSlayerAccessory(iClass)) {
            if (pTargetItem->getItemType() > pInfo->getSlayerLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isVampireWeapon(iClass) || isVampireArmor(iClass) || isVampireAccessory(iClass)) {
            if (pTargetItem->getItemType() > pInfo->getVampireLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else if (isOustersWeapon(iClass) || isOustersArmor(iClass) || isOustersAccessory(iClass)) {
            if (pTargetItem->getItemType() > pInfo->getOustersLevel()) {
                sendCannotUse(pPacket, pPlayer);
                return;
            }
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;

    default: {
        sendCannotUse(pPacket, pPlayer);
        return;
    }
    }

    list<OptionType_t> oList = pTargetItem->getOptionTypeList();
    if (oList.size() != 1) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    // Now remove the option.
    oList.clear();
    pTargetItem->setOptionType(oList);

    // Record in the DB that there is no option
    pTargetItem->tinysave("OptionType=''");

    pTargetItem->setTraceItem(bTraceLog(pTargetItem));

    Assert(pItem == pPC->getExtraInventorySlotItem());
    // The item the option is taken off is on the mouse. If not, never mind
    pPC->deleteItemFromExtraInventorySlot();
    if (pItem->isTraceItem()) {
        remainTraceLog(pItem, pPC->getName(), "ClearOption", ITEM_LOG_DELETE, DETAIL_ENCHANT);
    }
    pItem->destroy();
    SAFE_DELETE(pItem);

    GCAddItemToItemVerify gcVerify;
    gcVerify.setCode(ADD_ITEM_TO_ITEM_VERIFY_CLEAR_OPTION_OK);

    pPlayer->sendPacket(&gcVerify);

    __END_CATCH
}

void sendCannotUse(CGMixItem* pPacket, Player* pPlayer) {
    GCCannotUse _GCCannotUse;
    _GCCannotUse.setObjectID(pPacket->getObjectID());
    pPlayer->sendPacket(&_GCCannotUse);
    return;
}
#endif
