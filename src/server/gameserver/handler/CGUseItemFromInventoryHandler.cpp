//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUseItemFromInventoryHandler.cpp
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUseItemFromInventory.h"

#ifdef __GAME_SERVER__
#include <math.h>
#include <stdio.h>

#include "CreatureUtil.h"
#include "DynamicZone.h"
#include "Effect.h"
#include "EffectAftermath.h"
#include "EffectAutoTurret.h"
#include "EffectHPRecovery.h"
#include "EffectHasPet.h"
#include "EffectKillTimer.h"
#include "EffectMPRecovery.h"
#include "EffectManager.h"
#include "EffectReloadTimer.h"
#include "EffectTranslation.h"
#include "EffectTurretLaser.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddHelicopter.h"
#include "GCAddOusters.h"
#include "GCAddSlayer.h"
#include "GCAddVampire.h"
#include "GCAddressListVerify.h"
#include "GCCannotUse.h"
#include "GCCreateItem.h"
#include "GCDeleteObject.h"
#include "GCHPRecoveryStartToOthers.h"
#include "GCHPRecoveryStartToSelf.h"
#include "GCMPRecoveryStart.h"
#include "GCModifyInformation.h"
#include "GCMyStoreInfo.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCUseOK.h"
#include "GDRLairManager.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemMineInfo.h"
#include "ItemUtil.h"
#include "Monster.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "PetTypeInfo.h"
#include "PlayerCreature.h"
#include "SiegeManager.h"
#include "SimpleCreatureEffect.h"
#include "SkillInfo.h"
#include "Slayer.h"
#include "Store.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "item/DyePotion.h"
#include "item/EffectItem.h"
#include "item/EventETC.h"
#include "item/Key.h"
#include "item/OustersSummonItem.h"
#include "item/PetFood.h"
#include "item/PetItem.h"
#include "item/Potion.h"
#include "item/ResurrectItem.h"
#include "item/SMSItem.h"
#include "item/Serum.h"
#include "item/SlayerPortalItem.h"
#include "item/TrapItem.h"
#include "item/VampirePortalItem.h"
#include "repository/ItemObjectRepository.h"
#include "skill/EffectSummonSylph.h"
#include "skill/Skill.h"
#include "skill/SkillUtil.h"

bool changeHairColorEx(PlayerCreature* pPC, Color_t color);
bool changeBatColorEx(PlayerCreature* pPC, Color_t color);
bool changeMasterEffectColorEx(PlayerCreature* pPC, BYTE color);
bool changeSkinColorEx(PlayerCreature* pPC, Color_t color);
inline bool sendCannotUse(CGUseItemFromInventory* pPacket, Player* pPlayer);
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::execute(CGUseItemFromInventory* pPacket, Player* pPlayer)

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
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // It is an error when the inventory holds no such item.
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    if (pItem == NULL) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Get the Object of the item in the inventory.
    ObjectID_t ItemObjectID = pItem->getObjectID();

    // A mismatched OID, or an item that cannot be used, is an error.
    if (ItemObjectID != pPacket->getObjectID() || !isUsableItem(pItem, pCreature)) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }


    // Find out whether the item is in the store
    // Fix the bug that lets a skill card be used without limit in the store
    if (pItem->getItemType() >= 5 && pItem->getItemType() <= 7) {
        BYTE bIndex = pPC->getStore()->getItemIndex(pItem);
        if (bIndex != 0xff) {
            pPC->getStore()->removeStoreItem(bIndex);
            GCMyStoreInfo gcInfo;
            gcInfo.setStoreInfo(&(pPC->getStore()->getStoreInfo()));
            pGamePlayer->sendPacket(&gcInfo);
        }
    }
    if (pPC->getStore()->getItemIndex(pItem) != 0xff) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Branch to the handling function by item kind.

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_POTION:
        Assert(false);
        break;
    case Item::ITEM_CLASS_MAGAZINE:
        Assert(false);
        break;
    case Item::ITEM_CLASS_ETC:
        Assert(pItem->getItemType() != 0);
        executeTranslator(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_SERUM:
        Assert(false);
        break;
    case Item::ITEM_CLASS_VAMPIRE_ETC:
        Assert(pItem->getItemType() > 1);
        executeTranslator(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_SLAYER_PORTAL_ITEM:
        executeSlayerPortalItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM:
        executeOustersSummonItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_KEY:
        executeKeyItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_EVENT_ETC:
        executeFirecraker(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_DYE_POTION:
        executeDyePotion(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_RESURRECT_ITEM:
        executeResurrectItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_EFFECT_ITEM:
        executeEffectItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_PET_ITEM:
        executePetItem(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_PET_FOOD:
        executePetFood(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_EVENT_GIFT_BOX:
        executeEventGiftBox(pPacket, pPlayer);
        break;
    case Item::ITEM_CLASS_SMS_ITEM: {
        SMSItemInfo* pItemInfo = dynamic_cast<SMSItemInfo*>(
            de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
        Assert(pItemInfo != NULL);

        uint charge = pItemInfo->getCharge();
        pPC->setSMSCharge(pPC->getSMSCharge() + charge);

        GCUseOK _GCUseOK;
        pGamePlayer->sendPacket(&_GCUseOK);

        GCAddressListVerify gcVerify;
        gcVerify.setCode(GCAddressListVerify::SMS_CHARGE_OK);
        gcVerify.setParameter(pPC->getSMSCharge());
        pGamePlayer->sendPacket(&gcVerify);

        char buffer[100];
        snprintf(buffer, sizeof(buffer), "SMSCharge=%u", pPC->getSMSCharge());
        pPC->tinysave(buffer);

        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    } break;

    case Item::ITEM_CLASS_TRAP_ITEM:
        executeTrapItem(pPacket, pPlayer);
        break;
        // code that deducts the new skill card
    case Item::ITEM_CLASS_MOON_CARD:
        if (pItem->getItemType() >= 5 && pItem->getItemType() <= 7) {
            GCUseOK _GCUseOK;
            pGamePlayer->sendPacket(&_GCUseOK);
            decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        }
        break;
        // add end by Coffee 2007-6-9
    default:
        Assert(false);
        break;
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executePotion(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    // It cannot be used in the coma state.
    if (pSlayer->isFlag(Effect::EFFECT_CLASS_COMA)
        // Songpyeon can only be used by a paying user.
        || (pItem->getItemType() == 11 && !pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay())) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    HP_t MaxHP = pSlayer->getHP(ATTR_MAX);
    HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
    MP_t MaxMP = pSlayer->getMP(ATTR_MAX);
    MP_t CurrentMP = pSlayer->getMP(ATTR_CURRENT);
    Potion* pPotion = dynamic_cast<Potion*>(pItem);

    int HPQuantity = pPotion->getHPQuantity();
    int MPQuantity = pPotion->getMPQuantity();

    int HPDelayProvider = pPotion->getHPDelay();
    int MPDelayProvider = pPotion->getMPDelay();

    // With the Activation Effect on, the recovery rate doubles.
    if (pSlayer->isFlag(Effect::EFFECT_CLASS_ACTIVATION)) {
        if (pPotion->getItemType() >= 14 && pPotion->getItemType() <= 17) {
            // It can still be used.
        } else {
            HPDelayProvider = (HPDelayProvider >> 1);
            MPDelayProvider = (MPDelayProvider >> 1);

            HPDelayProvider = max(HPDelayProvider, 1);
            MPDelayProvider = max(MPDelayProvider, 1);
        }
    }

    Attr_t INT = pSlayer->getINT();

    int PotionHPAmount = 0, PotionMPAmount = 0;

    // Holding another race's Relic drops serum and potion effects to 50%.
    PotionHPAmount = pPotion->getHPAmount();
    PotionMPAmount = pPotion->getMPAmount();

    int HPAmount = min(MaxHP - CurrentHP, PotionHPAmount);
    int MPAmount = min(MaxMP - CurrentMP, (int)(PotionMPAmount * (double)(1 + (double)((double)INT / 300.0))));

    bool notRecoverHP = false;
    bool notRecoverMP = false;

    // If there is an HP recovery amount...
    if (HPAmount != 0 && HPQuantity != 0) {
        if (CurrentHP < MaxHP) {
            EffectManager* pEffectManager = pSlayer->getEffectManager();

            double temp = (double)((double)HPAmount / (double)HPQuantity);
            int Period = (uint)ceil(temp);
            Turn_t Deadline = Period * HPDelayProvider;

            if (pSlayer->isFlag(Effect::EFFECT_CLASS_HP_RECOVERY)) {
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_HP_RECOVERY);
                EffectHPRecovery* pEffectHPRecoveryEffect = dynamic_cast<EffectHPRecovery*>(pEffect);

                // Compute the HP the existing unit amount and count would fill.
                // Add that to the current recovery amount.
                int PrevHPAmount = pEffectHPRecoveryEffect->getHPQuantity() * pEffectHPRecoveryEffect->getPeriod();
                HPAmount = min((int)(HPAmount + PrevHPAmount), MaxHP - CurrentHP);

                // Take the larger unit recovery amount and the smaller delay of the two.
                HPQuantity = max(HPQuantity, (int)(pEffectHPRecoveryEffect->getHPQuantity()));
                HPDelayProvider = min(HPDelayProvider, (int)(pEffectHPRecoveryEffect->getDelay()));

                // From the current recovery amount, decide how much is recovered how many times.
                temp = (double)((double)HPAmount / (double)HPQuantity);
                Period = (uint)ceil(temp);
                Deadline = Period * HPDelayProvider;

                // Refresh the HP Recovery effect.
                pEffectHPRecoveryEffect->setDeadline(Deadline);
                pEffectHPRecoveryEffect->setDelay(HPDelayProvider);
                pEffectHPRecoveryEffect->setHPQuantity(HPQuantity);
                pEffectHPRecoveryEffect->setPeriod(Period);

                // Send the packet that starts the recovery to oneself.
                GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
                gcHPRecoveryStartToSelf.setPeriod(pEffectHPRecoveryEffect->getPeriod());
                gcHPRecoveryStartToSelf.setDelay(pEffectHPRecoveryEffect->getDelay());
                gcHPRecoveryStartToSelf.setQuantity(pEffectHPRecoveryEffect->getHPQuantity());

                pGamePlayer->sendPacket(&gcHPRecoveryStartToSelf);

                // Send the packet that starts the recovery to the others.
                // The recovery refresh packet is the same packet as the start one.
                GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
                gcHPRecoveryStartToOthers.setObjectID(pSlayer->getObjectID());
                gcHPRecoveryStartToOthers.setPeriod(pEffectHPRecoveryEffect->getPeriod());
                gcHPRecoveryStartToOthers.setDelay(pEffectHPRecoveryEffect->getDelay());
                gcHPRecoveryStartToOthers.setQuantity(pEffectHPRecoveryEffect->getHPQuantity());

                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcHPRecoveryStartToOthers, pSlayer);
                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            } else {
                EffectHPRecovery* pEffectHPRecovery = new EffectHPRecovery();

                pEffectHPRecovery->setTarget(pSlayer);
                pEffectHPRecovery->setDeadline(Deadline);
                pEffectHPRecovery->setDelay(HPDelayProvider);
                pEffectHPRecovery->setNextTime(0);
                pEffectHPRecovery->setHPQuantity(HPQuantity);
                pEffectHPRecovery->setPeriod(Period);

                pEffectManager->addEffect(pEffectHPRecovery);

                // Send the packet that starts the recovery to oneself.
                GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
                gcHPRecoveryStartToSelf.setPeriod(Period);
                gcHPRecoveryStartToSelf.setDelay(HPDelayProvider);
                gcHPRecoveryStartToSelf.setQuantity(HPQuantity);

                pGamePlayer->sendPacket(&gcHPRecoveryStartToSelf);

                // Send the packet that starts the recovery to those who can see.
                GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
                gcHPRecoveryStartToOthers.setObjectID(pSlayer->getObjectID());
                gcHPRecoveryStartToOthers.setPeriod(Period);
                gcHPRecoveryStartToOthers.setDelay(HPDelayProvider);
                gcHPRecoveryStartToOthers.setQuantity(HPQuantity);

                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcHPRecoveryStartToOthers, pSlayer);
                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            }

            decreaseItemNum(pItem, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        } else {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(pPacket->getObjectID());
            pGamePlayer->sendPacket(&_GCCannotUse);
            return;
        }
    } else {
        notRecoverHP = true;
    }

    // If there is an MP recovery amount...
    if (MPAmount != 0 && MPQuantity != 0) {
        if (CurrentMP < MaxMP) {
            EffectManager* pEffectManager = pSlayer->getEffectManager();

            double temp = (double)((double)MPAmount / (double)MPQuantity);
            uint Period = (uint)ceil(temp);
            Turn_t Deadline = Period * MPDelayProvider;

            if (pSlayer->isFlag(Effect::EFFECT_CLASS_MP_RECOVERY)) {
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_MP_RECOVERY);
                EffectMPRecovery* pEffectMPRecoveryEffect = dynamic_cast<EffectMPRecovery*>(pEffect);

                // Compute the MP the existing unit amount and count would fill.
                // Add that to the current recovery amount.
                int PrevMPAmount = pEffectMPRecoveryEffect->getMPQuantity() * pEffectMPRecoveryEffect->getPeriod();
                MPAmount = min((int)(MPAmount + PrevMPAmount), MaxMP - CurrentMP);

                // Take the larger unit recovery amount and the smaller delay of the two.
                MPQuantity = max(MPQuantity, (int)(pEffectMPRecoveryEffect->getMPQuantity()));
                MPDelayProvider = min(MPDelayProvider, (int)(pEffectMPRecoveryEffect->getDelay()));

                // From the current recovery amount, decide how much is recovered how many times.
                temp = (double)((double)MPAmount / (double)MPQuantity);
                Period = (uint)ceil(temp);
                Deadline = Period * MPDelayProvider;

                // Refresh the MP Recovery effect.
                pEffectMPRecoveryEffect->setDeadline(Deadline);
                pEffectMPRecoveryEffect->setDelay(MPDelayProvider);
                pEffectMPRecoveryEffect->setMPQuantity(MPQuantity);
                pEffectMPRecoveryEffect->setPeriod(Period);

                // Send the packet that starts the recovery to oneself.
                GCMPRecoveryStart gcMPRecoveryStart;
                gcMPRecoveryStart.setPeriod(pEffectMPRecoveryEffect->getPeriod());
                gcMPRecoveryStart.setDelay(pEffectMPRecoveryEffect->getDelay());
                gcMPRecoveryStart.setQuantity(pEffectMPRecoveryEffect->getMPQuantity());

                pGamePlayer->sendPacket(&gcMPRecoveryStart);

                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            } else {
                EffectMPRecovery* pEffectMPRecovery = new EffectMPRecovery();

                pEffectMPRecovery->setTarget(pSlayer);
                pEffectMPRecovery->setDeadline(Deadline);
                pEffectMPRecovery->setDelay(MPDelayProvider);
                pEffectMPRecovery->setNextTime(0);
                pEffectMPRecovery->setMPQuantity(MPQuantity);
                pEffectMPRecovery->setPeriod(Period);

                pEffectManager->addEffect(pEffectMPRecovery);

                // Send the packet that starts the recovery to oneself.
                GCMPRecoveryStart gcMPRecoveryStart;
                gcMPRecoveryStart.setPeriod(Period);
                gcMPRecoveryStart.setDelay(MPDelayProvider);
                gcMPRecoveryStart.setQuantity(MPQuantity);

                pGamePlayer->sendPacket(&gcMPRecoveryStart);

                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            }

            decreaseItemNum(pItem, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        } else {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(pPacket->getObjectID());
            pGamePlayer->sendPacket(&_GCCannotUse);
            return;
        }
    } else {
        notRecoverMP = true;
    }

    if (notRecoverHP && notRecoverMP) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeMagazine(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    ObjectID_t ItemObjectID = pItem->getObjectID();

    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Item* pArmsItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    bool bSuccess = false;

    if (pArmsItem != NULL) {
        if (isArmsWeapon(pArmsItem)) {
            SkillSlot* pVivid = pSlayer->getSkill(SKILL_VIVID_MAGAZINE);
            bool hasVivid = (pVivid != NULL) && pVivid->canUse();

            if (isSuitableMagazine(pArmsItem, pItem, hasVivid))
                bSuccess = true;
        }
    }

    // There is a reload delay, so register it with an effect.
    EffectManager* pEffectManager = pSlayer->getEffectManager();
    if (pEffectManager == NULL)
        return;

    if (bSuccess && !pSlayer->isFlag(Effect::EFFECT_CLASS_RELOAD_TIMER)) {
        EffectReloadTimer* pEffect = new EffectReloadTimer(pSlayer);

        pEffect->setFromInventory(true);
        pEffect->setObjectID(ItemObjectID);
        pEffect->setInventoryXY(InvenX, InvenY);

        // 1 second --> 0.7 seconds
        if (pSlayer->hasSkill(SKILL_FAST_RELOAD))
            pEffect->setDeadline(7); // fast reload (0.7 sec)
        else
            pEffect->setDeadline(2 * 10); // ordinary reload (2sec)

        pSlayer->setFlag(Effect::EFFECT_CLASS_RELOAD_TIMER);
        pEffectManager->addEffect(pEffect);
    } else {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotUse);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeETC(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    // When the item is a stacking kind,
    // as much as was used is deleted.
    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeSerum(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__


        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    Assert(pCreature->isVampire());

    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

    // It cannot be used in the coma state.
    if (pVampire->isFlag(Effect::EFFECT_CLASS_COMA)
        // Songpyeon can only be used by a paying user.
        || (pItem->getItemType() == 5 && !pGamePlayer->isPayPlaying() && !pGamePlayer->isPremiumPlay())) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    HP_t MaxHP = pVampire->getHP(ATTR_MAX);
    HP_t CurrentHP = pVampire->getHP(ATTR_CURRENT);
    Serum* pSerum = dynamic_cast<Serum*>(pItem);
    int RegenHP = 0;

    RegenHP = pSerum->getHPAmount();

    int RegenPeriod = pSerum->getPeriod() * 10; // the period of the unit time
    int RegenCount = pSerum->getCount();        // how many unit times are repeated?

    int RegenHPUnit = (int)((float)MaxHP * (float)RegenHP * 0.01); // the HP recovered at a time
    int HPAmount = min(MaxHP - CurrentHP, RegenHPUnit * RegenCount);

    // If there is an HP recovery amount...
    if (HPAmount != 0) {
        // How much, how many times, every how many seconds.
        if (CurrentHP < MaxHP) {
            EffectManager* pEffectManager = pVampire->getEffectManager();
            Turn_t Period = RegenCount;             // how many times does it recover?
            Turn_t Deadline = RegenPeriod * Period; // when does it end?

            if (pVampire->isFlag(Effect::EFFECT_CLASS_HP_RECOVERY)) {
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_HP_RECOVERY);
                EffectHPRecovery* pEffectHPRecoveryEffect = dynamic_cast<EffectHPRecovery*>(pEffect);

                // Refresh how many more times it has to run.
                Turn_t OldCount = pEffectHPRecoveryEffect->getPeriod();
                Turn_t NewPeriod = OldCount + Period;
                pEffectHPRecoveryEffect->setPeriod(NewPeriod);
                pEffectHPRecoveryEffect->setDeadline(NewPeriod * RegenPeriod);

                // Send the packet that starts the recovery to oneself.
                GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
                gcHPRecoveryStartToSelf.setPeriod(NewPeriod);     // how many times does it recover?
                gcHPRecoveryStartToSelf.setDelay(RegenPeriod);    // in what second steps?
                gcHPRecoveryStartToSelf.setQuantity(RegenHPUnit); // how much does it recover at a time?

                pGamePlayer->sendPacket(&gcHPRecoveryStartToSelf);

                // Send the packet that starts the recovery to the others.
                // The recovery refresh packet is the same packet as the start one.
                GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
                gcHPRecoveryStartToOthers.setObjectID(pVampire->getObjectID());
                gcHPRecoveryStartToOthers.setPeriod(NewPeriod);
                gcHPRecoveryStartToOthers.setDelay(RegenPeriod);
                gcHPRecoveryStartToOthers.setQuantity(RegenHPUnit);

                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcHPRecoveryStartToOthers, pVampire);
                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            } else {
                EffectHPRecovery* pEffectHPRecovery = new EffectHPRecovery();

                pEffectHPRecovery->setTarget(pVampire);
                pEffectHPRecovery->setDeadline(Deadline);
                pEffectHPRecovery->setDelay(RegenPeriod);
                pEffectHPRecovery->setNextTime(0);
                pEffectHPRecovery->setHPQuantity(RegenHPUnit);
                pEffectHPRecovery->setPeriod(Period);

                pEffectManager->addEffect(pEffectHPRecovery);

                // Send the packet that starts the recovery to oneself.
                GCHPRecoveryStartToSelf gcHPRecoveryStartToSelf;
                gcHPRecoveryStartToSelf.setPeriod(Period);
                gcHPRecoveryStartToSelf.setDelay(RegenPeriod);
                gcHPRecoveryStartToSelf.setQuantity(RegenHPUnit);

                pGamePlayer->sendPacket(&gcHPRecoveryStartToSelf);

                // Send the packet that starts the recovery to those who can see.
                GCHPRecoveryStartToOthers gcHPRecoveryStartToOthers;
                gcHPRecoveryStartToOthers.setObjectID(pVampire->getObjectID());
                gcHPRecoveryStartToOthers.setPeriod(Period);
                gcHPRecoveryStartToOthers.setDelay(RegenPeriod);
                gcHPRecoveryStartToOthers.setQuantity(RegenHPUnit);

                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &gcHPRecoveryStartToOthers, pVampire);

                GCUseOK _GCUseOK;
                pGamePlayer->sendPacket(&_GCUseOK);
            }
        } else {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(pPacket->getObjectID());
            pGamePlayer->sendPacket(&_GCCannotUse);
            return;
        }
    } else {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    decreaseItemNum(pItem, pInventory, pVampire->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);

#endif

    __END_DEBUG_EX __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeVampireETC(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);


#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeSlayerPortalItem(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    SlayerPortalItem* pPortalItem = dynamic_cast<SlayerPortalItem*>(pItem);

    Assert(pSlayer != NULL);
    Assert(pPortalItem != NULL);

    Store* pStore = pSlayer->getStore();
    if (pStore->isOpen()) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // A helicopter cannot be called while holding a Relic.
    bool bHasRelic = false;
    if (pSlayer->hasRelicItem() || pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
        pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
        bHasRelic = true;
    }

    // A helicopter cannot be called while petrified.
    bool bParalyze = pSlayer->isFlag(Effect::EFFECT_CLASS_PARALYZE) ? true : false;

    bool bZoneTypeCheck = (pZone->getZoneType() == ZONE_NORMAL_FIELD) ? true : false;
    bool bCanUseCheck = pSlayer->isRealWearing(pPortalItem);
    bool bChargeCheck = (pPortalItem->getCharge() > 0) ? true : false;
    bool bZoneCheck = pZone->isNoPortalZone();

    if (bZoneTypeCheck && bCanUseCheck && bChargeCheck && !bHasRelic && !bParalyze && !bZoneCheck) {
        // Lower the item's charge and attach the effect to the slayer.
        pPortalItem->setCharge(pPortalItem->getCharge() - 1);
        // pPortalItem->save(pSlayer->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY); // save the item information.
        //  Item save optimization.
        char pField[80];
        snprintf(pField, sizeof(pField), "Charge=%d", pPortalItem->getCharge());
        pPortalItem->tinysave(pField);

        pSlayer->setFlag(Effect::EFFECT_CLASS_SLAYER_PORTAL);

        // Broadcast the packet that adds the helicopter around.
        GCAddHelicopter gcAddHelicopter;
        gcAddHelicopter.setObjectID(pSlayer->getObjectID());
        gcAddHelicopter.setCode(0);
        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcAddHelicopter);

        // Send the verify packet.
        GCUseOK gcUseOK;
        pPlayer->sendPacket(&gcUseOK);
    } else {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

void CGUseItemFromInventoryHandler::executeOustersSummonItem(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_TRY

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    if (SiegeManager::Instance().isSiegeZone(pPC->getZoneID()) || pZone->isNoPortalZone()) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    Assert(pCreature->isOusters());

    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
    OustersSummonItem* pSummonItem = dynamic_cast<OustersSummonItem*>(pItem);

    Assert(pOusters != NULL);
    Assert(pSummonItem != NULL);

    // A helicopter cannot be called while holding a Relic.
    bool bHasRelic = false;
    if (pOusters->hasRelicItem() || pOusters->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
        pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) ||
        GDRLairManager::Instance().isGDRLairZone(pZone->getZoneID()) ||
        (pZone->isDynamicZone() && pZone->getDynamicZone()->getTemplateZoneID() == 4002)) {
        bHasRelic = true;
    }

    // A helicopter cannot be called while petrified.
    bool bParalyze = pOusters->isFlag(Effect::EFFECT_CLASS_PARALYZE) ? true : false;

    bool bCanUseCheck = pOusters->isRealWearing(pSummonItem);
    bool bChargeCheck = (pSummonItem->getCharge() > 0) ? true : false;

    SkillType_t SkillType = SKILL_SUMMON_SYLPH;
    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
    OustersSummonItemInfo* pItemInfo = dynamic_cast<OustersSummonItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    Assert(pItemInfo != NULL);

    int RequireMP = (int)pSkillInfo->getConsumeMP();
    bool bManaCheck = hasEnoughMana(pOusters, RequireMP);
    bool bTileCheck = checkZoneLevelToUseSkill(pOusters);
    bool bEffect = pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH);
    bool bSatisfyRequire = pOusters->satisfySkillRequire(pSkillInfo);
    bool bRangeCheck = checkZoneLevelToUseSkill(pOusters);

    if (bCanUseCheck && bChargeCheck && !bHasRelic && !bParalyze && bTileCheck && bManaCheck && !bEffect &&
        bSatisfyRequire && bRangeCheck) {
        GCModifyInformation gcMI;

        decreaseMana(pOusters, RequireMP, gcMI);
        // Lower the item's charge and attach the effect to the slayer.
        pSummonItem->setCharge(pSummonItem->getCharge() - 1);
        // pSummonItem->save(pOusters->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY); // save the item information.
        //  Item save optimization.
        char pField[80];
        snprintf(pField, sizeof(pField), "Charge=%d", pSummonItem->getCharge());
        pSummonItem->tinysave(pField);

        // Build the effect class and attach it.
        EffectSummonSylph* pEffect = new EffectSummonSylph(pOusters);
        pEffect->setEClass((Effect::EffectClass)pItemInfo->getEffectID());
        pOusters->addEffect(pEffect);
        pOusters->setFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH);

        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);
        pOusters->initAllStat();
        pOusters->addModifyInfo(prev, gcMI);

        // Send the verify packet.
        GCUseOK gcUseOK;
        pPlayer->sendPacket(&gcUseOK);
        pPlayer->sendPacket(&gcMI);

        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pOusters->getObjectID());
        gcAddEffect.setEffectID(pEffect->getSendEffectClass());
        gcAddEffect.setDuration(pEffect->getRemainDuration());

        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcAddEffect, pOusters);
        pPlayer->sendPacket(&gcAddEffect);

        if (pOusters->getPetInfo() != NULL) {
            pOusters->setPetInfo(NULL);
            sendPetInfo(pGamePlayer, true);
        }

        pOusters->getGQuestManager()->rideMotorcycle();
    } else {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }


#endif

    __END_DEBUG_EX __END_CATCH
}


void CGUseItemFromInventoryHandler::executeKeyItem(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__


        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    ParkingCenter& parking = de::gameContext().parking();

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    if (SiegeManager::Instance().isSiegeZone(pPC->getZoneID())) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Only a SLAYER can call a motorcycle.
    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

    // The summonable state has to be set.
    // It cannot be used in the coma state.
    // A motorcycle cannot be called when the item is not a key item.
    // No motorcycle summon in a master lair or a portal-free area
    // Only a premium user can call one.
    if (!de::gameContext().variables().isSummonMotorcycle() || pSlayer->hasRideMotorcycle() ||
        pItem->getItemClass() != Item::ITEM_CLASS_KEY || pSlayer->isFlag(Effect::EFFECT_CLASS_COMA) ||
        (pZone->getZoneLevel(pCreature->getX(), pCreature->getY()) & SAFE_ZONE) || pZone->isMasterLair() ||
        pZone->isNoPortalZone() || (!pGamePlayer->isPremiumPlay() && !pGamePlayer->isPayPlaying()) ||
        de::gameContext().flags().isInPoleField(ZONE_COORD(pZone->getZoneID(), pCreature->getX(), pCreature->getY())) ||
        GDRLairManager::Instance().isGDRLairZone(pZone->getZoneID())) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    // Check whether the item already exists.
    ItemID_t targetID = dynamic_cast<Key*>(pItem)->getTarget();

    // A targetID of 0 means targetID (the motorcycleObject's ItemID) was never set.
    // In that case the targetID can provisionally be made equal to the key's ItemID.
    // Since targetID goes in as the motorcycle's itemID,
    // broadcasting and the like seem to have crashed on an Assert().
    // by sigi. 2002.12.25 x-mas T_T;
    if (targetID == 0) {
        Key* pKey = dynamic_cast<Key*>(pItem);
        Assert(pKey != NULL);

        targetID = pKey->setNewMotorcycle(pSlayer);
    } else {
        // Once a motorcycle and a key are linked, someone keeps deleting the motorcycle.
        // Check that the motorcycle linked to the key really is in the DB, and if not make a new one.
        if (!defaultItemObjectRepository().motorcycleExists(targetID)) {
            Key* pKey = dynamic_cast<Key*>(pItem);
            Assert(pKey != NULL);

            targetID = pKey->setNewMotorcycle(pSlayer);
        }
    }


    // Last-ditch defensive code
    if (targetID == 0) {
        filelog("errorLog.txt", "[ActionRedeemMotorcycle] itemID=%lu, motorItemID=%lu", (int)pItem->getItemID(),
                (int)targetID);
        return;
    }


    if (parking.hasMotorcycleBox(targetID)) {
        MotorcycleBox* pMotorcycleBox = parking.getMotorcycleBox(targetID);

        if (pMotorcycleBox != NULL && !pMotorcycleBox->isTransport()) {
            Zone* pMotorZone = pMotorcycleBox->getZone();
            ZoneCoord_t motorX = pMotorcycleBox->getX();
            ZoneCoord_t motorY = pMotorcycleBox->getY();
            Motorcycle* pMotorcycle = pMotorcycleBox->getMotorcycle();

            // When it is in the same zone
            // Do not call it when the distance is too short.
            if (pMotorZone != pZone || pSlayer->getDistance(motorX, motorY) > 15) {
                // Mark it as moving to another zone.
                pMotorcycleBox->setTransport();

                // Move the motorcycle to the slayer's zone.
                pMotorZone->transportItem(motorX, motorY, pMotorcycle, pZone, pSlayer->getX(), pSlayer->getY());

                // This stands in for Use OK.
                // A Use would probably make the item disappear.

                // A delay should be applied for a while..
            }
        }

        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);

        return;
    }

    {
        MotorcycleRedeemRow redeemRow;
        bool bFound =
            defaultItemObjectRepository().loadMotorcycleForRedeem(REDEEM_SPELLING_HANDLER, targetID, redeemRow);

        ItemID_t itemID;
        ItemType_t itemType;
        list<OptionType_t> optionTypes;
        Durability_t durability;

        if (bFound) {
            itemID = redeemRow.itemID;
            itemType = redeemRow.itemType;

            setOptionTypeFromField(optionTypes, redeemRow.optionField);

            durability = redeemRow.durability;
        } else {
            itemID = targetID;
            itemType = 0;
            durability = 300;
        }

        // Create a new motorcycle object.
        Motorcycle* pMotorcycle = new Motorcycle(itemType, optionTypes);

        Assert(pMotorcycle != NULL);

        pMotorcycle->setItemID(itemID);
        pMotorcycle->setDurability(durability);

        // Get an Object ID
        (pZone->getObjectRegistry()).registerObject(pMotorcycle);


        // Add the motorcycle to the zone.
        TPOINT pt = pZone->addItem(pMotorcycle, pSlayer->getX(), pSlayer->getY(), false);

        if (pt.x == -1) {
            StringStream msg;
            msg << "Cannot add the motorcycle to the zone: " << "ZoneID=" << (int)pZone->getZoneID()
                << ", X=" << (int)pSlayer->getX() << ", Y=" << (int)pSlayer->getY();

            filelog("motorError.txt", "%s", msg.toString().c_str());
            // throw Error("The motorcycle cannot be put into the zone");

            SAFE_DELETE(pMotorcycle);
        } else {
            if (!bFound) {
                // by sigi. 2002.10.14
                defaultItemObjectRepository().insertRedeemedMotorcycle(
                    REDEEM_SPELLING_HANDLER, itemID, pMotorcycle->getObjectID(), itemType, STORAGE_ZONE,
                    pZone->getZoneID(), pt.x, pt.y, durability);
            }

            // Register the motorcycle with the Parking Center.
            MotorcycleBox* pBox = new MotorcycleBox(pMotorcycle, pZone, pt.x, pt.y);
            Assert(pBox != NULL);
            parking.addMotorcycleBox(pBox);
        }


        // This is the verify packet.
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
    }
#endif
    __END_DEBUG_EX __END_CATCH
}

#ifdef __GAME_SERVER__

static const Effect::EffectClass FirecrackerEffects[] = {
    Effect::EFFECT_CLASS_FIRE_CRACKER_1,             // 0
    Effect::EFFECT_CLASS_FIRE_CRACKER_2,             // 1
    Effect::EFFECT_CLASS_FIRE_CRACKER_3,             // 2
    Effect::EFFECT_CLASS_DRAGON_FIRE_CRACKER,        // 3
    Effect::EFFECT_CLASS_FIRE_CRACKER_4,             // 4
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_1,      // 5
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_2,      // 6
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_3,      // 7
    Effect::EFFECT_CLASS_FIRE_CRACKER_VOLLEY_4,      // 8
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_1, // 9
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_2, // 10
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_3, // 11
    Effect::EFFECT_CLASS_FIRE_CRACKER_WIDE_VOLLEY_4, // 12
    Effect::EFFECT_CLASS_FIRE_CRACKER_STORM          // 13
};

#endif

void CGUseItemFromInventoryHandler::executeFirecraker(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__


        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    ObjectID_t ItemObjectID = pItem->getObjectID();

    if (pItem->getItemType() >= 14) {
        EventETCInfo* pInfo = dynamic_cast<EventETCInfo*>(
            de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
        Assert(pInfo != NULL);

        int amount = pInfo->getFunction();

        GCModifyInformation gcMI;
        bool HPRegen = false, MPRegen = false;
        HP_t CurrentHP = 0, MaxHP = 0;
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            CurrentHP = pSlayer->getHP();
            MaxHP = pSlayer->getHP(ATTR_MAX);
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            CurrentHP = pVampire->getHP();
            MaxHP = pVampire->getHP(ATTR_MAX);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            CurrentHP = pOusters->getHP();
            MaxHP = pOusters->getHP(ATTR_MAX);
        }

        if (CurrentHP < MaxHP) {
            CurrentHP += min(amount, MaxHP - CurrentHP);
            GCStatusCurrentHP gcHP;
            gcHP.setObjectID(pPC->getObjectID());
            gcHP.setCurrentHP(CurrentHP);
            pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcHP);

            gcMI.addLongData(MODIFY_CURRENT_HP, CurrentHP);
            HPRegen = true;

            if (pPC->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                pSlayer->setHP(CurrentHP);
            } else if (pPC->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
                pVampire->setHP(CurrentHP);
            } else if (pPC->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                pOusters->setHP(CurrentHP);
            }
        }

        MP_t CurrentMP = 0, MaxMP = 0;
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            CurrentMP = pSlayer->getMP();
            MaxMP = pSlayer->getMP(ATTR_MAX);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            CurrentMP = pOusters->getMP();
            MaxMP = pOusters->getMP(ATTR_MAX);
        }

        if (CurrentMP < MaxMP) {
            CurrentMP += min(amount, MaxMP - CurrentMP);
            gcMI.addLongData(MODIFY_CURRENT_MP, CurrentMP);
            MPRegen = true;

            if (pPC->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                pSlayer->setMP(CurrentMP);
            } else if (pPC->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                pOusters->setMP(CurrentMP);
            }
        }

        if (!HPRegen && !MPRegen) {
            sendCannotUse(pPacket, pPlayer);
            return;
        } else {
            pPlayer->sendPacket(&gcMI);
        }
    } else {
        // Take it for a skill used on a tile and check whether it can be used.
        // It cannot be used in a safe zone.
        if (!isAbleToUseTileSkill(pCreature) ||
            (pZone->getZoneLevel(pCreature->getX(), pCreature->getY()) & COMPLETE_SAFE_ZONE) ||
            ItemObjectID != pPacket->getObjectID()) {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(pPacket->getObjectID());
            pGamePlayer->sendPacket(&_GCCannotUse);
            return;
        }

        Effect::EffectClass effectClass = FirecrackerEffects[pItem->getItemType()];

        // Build the effect and broadcast it.
        GCAddEffectToTile gcAddEffectToTile;
        gcAddEffectToTile.setObjectID(pCreature->getObjectID());
        gcAddEffectToTile.setEffectID(effectClass);
        gcAddEffectToTile.setXY(pCreature->getX(), pCreature->getY());
        gcAddEffectToTile.setDuration(10); // no real meaning, just 1 second

        pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffectToTile);
    }

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

    // Tell the client that the item was used.
    GCUseOK gcUseOK;
    pGamePlayer->sendPacket(&gcUseOK);

#endif
    __END_DEBUG_EX __END_CATCH
}

void CGUseItemFromInventoryHandler::executeDyePotion(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    DyePotionInfo* pItemInfo = dynamic_cast<DyePotionInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
    bool bInitAllStat = false;
    bool bRefresh = true;

    if (pItem->getObjectID() != pPacket->getObjectID() || pItemInfo == NULL) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    BYTE func = pItemInfo->getFunctionFlag();
    int funcv = pItemInfo->getFunctionValue();

    if (func == DyePotionInfo::FUNCTION_HAIR && pPC->isVampire()) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    if (func == DyePotionInfo::FUNCTION_SKIN && pPC->isOusters()) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    switch (func) {
    case DyePotionInfo::FUNCTION_HAIR: {
        if (!changeHairColorEx(pPC, funcv)) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;
    case DyePotionInfo::FUNCTION_SKIN: {
        if (!changeSkinColorEx(pPC, funcv)) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;
    case DyePotionInfo::FUNCTION_SEX: {
        int code = changeSexEx(pPC);
        if (code != 0) {
            // Only the sex-change item puts the error code into the object id.
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID((ObjectID_t)code);
            pPlayer->sendPacket(&_GCCannotUse);

            return;
        } else
            bInitAllStat = true;
    } break;
    case DyePotionInfo::FUNCTION_BAT: {
        if (!changeBatColorEx(pPC, funcv)) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;
    case DyePotionInfo::FUNCTION_MASTER_EFFECT: {
        if (!changeMasterEffectColorEx(pPC, (BYTE)funcv)) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    } break;
    case DyePotionInfo::FUNCTION_REGEN: {
        GCModifyInformation gcMI;
        bool HPRegen = false, MPRegen = false;
        HP_t CurrentHP = 0, MaxHP = 0;
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            CurrentHP = pSlayer->getHP();
            MaxHP = pSlayer->getHP(ATTR_MAX);
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            CurrentHP = pVampire->getHP();
            MaxHP = pVampire->getHP(ATTR_MAX);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            CurrentHP = pOusters->getHP();
            MaxHP = pOusters->getHP(ATTR_MAX);
        }

        if (CurrentHP < MaxHP) {
            CurrentHP += min(1000, MaxHP - CurrentHP);
            GCStatusCurrentHP gcHP;
            gcHP.setObjectID(pPC->getObjectID());
            gcHP.setCurrentHP(CurrentHP);
            pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcHP);

            gcMI.addLongData(MODIFY_CURRENT_HP, CurrentHP);
            HPRegen = true;

            if (pPC->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                pSlayer->setHP(CurrentHP);
            } else if (pPC->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
                pVampire->setHP(CurrentHP);
            } else if (pPC->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                pOusters->setHP(CurrentHP);
            }
        }

        MP_t CurrentMP = 0, MaxMP = 0;
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            CurrentMP = pSlayer->getMP();
            MaxMP = pSlayer->getMP(ATTR_MAX);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            CurrentMP = pOusters->getMP();
            MaxMP = pOusters->getMP(ATTR_MAX);
        }

        if (CurrentMP < MaxMP) {
            CurrentMP += min(1000, MaxMP - CurrentMP);
            gcMI.addLongData(MODIFY_CURRENT_MP, CurrentMP);
            MPRegen = true;

            if (pPC->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                pSlayer->setMP(CurrentMP);
            } else if (pPC->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
                pOusters->setMP(CurrentMP);
            }
        }

        if (!HPRegen && !MPRegen) {
            sendCannotUse(pPacket, pPlayer);
            return;
        } else {
            pPlayer->sendPacket(&gcMI);
        }

        bRefresh = false;
    } break;
    default: {
        sendCannotUse(pPacket, pPlayer);
        return;
    }
    }

    GCUseOK gcUseOK;
    pPlayer->sendPacket(&gcUseOK);

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

    if (bInitAllStat) {
        pPC->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        transportCreature(pPC, pPC->getZoneID(), pPC->getX(), pPC->getY(), false);
    } else if (bRefresh) {
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pPC->getObjectID());
        pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcDeleteObject, pPC);

        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            Assert(pSlayer != NULL);

            GCAddSlayer gcAddSlayer;
            makeGCAddSlayer(&gcAddSlayer, pSlayer);
            pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddSlayer, pPC);
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            Assert(pVampire != NULL);

            GCAddVampire gcAddVampire;
            makeGCAddVampire(&gcAddVampire, pVampire);
            pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddVampire, pPC);
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            Assert(pOusters != NULL);

            GCAddOusters gcAddOusters;
            makeGCAddOusters(&gcAddOusters, pOusters);
            pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddOusters, pPC);
        } else {
            Assert(false);
        }
    }

    return;

#endif

    __END_DEBUG_EX __END_CATCH
}

#ifdef __GAME_SERVER__

bool changeHairColorEx(PlayerCreature* pPC, Color_t color) {
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        if (pSlayer->getHairColor() == color)
            return false;

        pSlayer->setHairColor(color);

        char query[25];
        snprintf(query, sizeof(query), "HairColor=%u", color);

        pSlayer->tinysave(query);

        return true;
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        Assert(pOusters != NULL);

        if (pOusters->getHairColor() == color)
            return false;

        pOusters->setHairColor(color);

        char query[25];
        snprintf(query, sizeof(query), "HairColor=%u", color);

        pOusters->tinysave(query);

        return true;
    }

    return false;
}

bool changeBatColorEx(PlayerCreature* pPC, Color_t color) {
    if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        if (pVampire->getBatColor() == color)
            return false;
        if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT))
            return false;

        pVampire->setBatColor(color);

        char query[25];
        snprintf(query, sizeof(query), "BatColor=%u", color);

        pVampire->tinysave(query);

        return true;
    }

    return false;
}

bool changeMasterEffectColorEx(PlayerCreature* pPC, BYTE color) {
    if (pPC->getLevel() >= 100 || pPC->isAdvanced()) {
        if (!pPC->canChangeMasterEffectColor())
            return false;

        if (pPC->getMasterEffectColor() == color)
            return false;

        pPC->setMasterEffectColor(color);

        char query[25];
        snprintf(query, sizeof(query), "MasterEffectColor=%u", color);

        pPC->tinysave(query);

        return true;
    }


    return false;
}

bool changeSkinColorEx(PlayerCreature* pPC, Color_t color) {
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        if (pSlayer->getSkinColor() == color)
            return false;

        pSlayer->setSkinColor(color);

        char query[25];
        snprintf(query, sizeof(query), "SkinColor=%u", color);

        pSlayer->tinysave(query);

        return true;
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        if (pVampire->getSkinColor() == color)
            return false;

        pVampire->setSkinColor(color);

        char query[25];
        snprintf(query, sizeof(query), "SkinColor=%u", color);

        pVampire->tinysave(query);

        return true;
    }

    return false;
}

bool sendCannotUse(CGUseItemFromInventory* pPacket, Player* pPlayer) {
    GCCannotUse _GCCannotUse;
    _GCCannotUse.setObjectID(pPacket->getObjectID());
    pPlayer->sendPacket(&_GCCannotUse);

    return true;
}

#endif

void CGUseItemFromInventoryHandler::executeResurrectItem(CGUseItemFromInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    ResurrectItemInfo* pItemInfo = dynamic_cast<ResurrectItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));


    if (pItem->getObjectID() != pPacket->getObjectID() || pItemInfo == NULL) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    if (!pPC->isFlag(Effect::EFFECT_CLASS_COMA)) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    Slayer* pSlayer = NULL;
    Vampire* pVampire = NULL;
    Ousters* pOusters = NULL;

    if (pPC->isSlayer())
        pSlayer = dynamic_cast<Slayer*>(pPC);
    else if (pPC->isVampire())
        pVampire = dynamic_cast<Vampire*>(pPC);
    else if (pPC->isOusters())
        pOusters = dynamic_cast<Ousters*>(pPC);

    ResurrectItemInfo::ResurrectType type = pItemInfo->getResurrectType();
    HP_t hp = 0;
    GCModifyInformation gcMI;
    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pPC->getObjectID());

    switch (type) {
    case ResurrectItemInfo::HP_1: {
        if (pPC->isSlayer()) {
            Assert(pSlayer != NULL);
            pSlayer->setHP(1);
            hp = 1;
            gcMI.addShortData(MODIFY_CURRENT_HP, hp);
        } else if (pPC->isVampire()) {
            Assert(pVampire != NULL);
            pVampire->setHP(1);
            hp = 1;
            gcMI.addShortData(MODIFY_CURRENT_HP, hp);
        } else if (pPC->isOusters()) {
            Assert(pOusters != NULL);
            pOusters->setHP(1);
            hp = 1;
            gcMI.addShortData(MODIFY_CURRENT_HP, hp);
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        if (GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID())) {
            filelog("GDRLair.log", "%s used a resurrection scroll in zone %d.", pPC->getName().c_str(),
                    pPC->getZoneID());
        }
    } break;
    case ResurrectItemInfo::HP_FULL: {
        if (pPC->isSlayer()) {
            Assert(pSlayer != NULL);
            pSlayer->setHP(pSlayer->getHP(ATTR_MAX));
            hp = pSlayer->getHP();

            gcMI.addShortData(MODIFY_CURRENT_HP, hp);

            if (pSlayer->getMP() < pSlayer->getMP(ATTR_MAX)) {
                pSlayer->setMP(pSlayer->getMP(ATTR_MAX));
                gcMI.addShortData(MODIFY_CURRENT_MP, pSlayer->getMP());
            }

            Effect* pEffect = pSlayer->getEffectManager()->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
            if (pEffect != NULL) {
                pEffect->destroy(pSlayer->getName());
                pSlayer->getEffectManager()->deleteEffect(pSlayer, Effect::EFFECT_CLASS_BLOOD_DRAIN);

                gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_BLOOD_DRAIN);

                // Attach the aftermath effect that discourages blood-drain farming.
                if (pSlayer->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                    Effect* pEffect = pSlayer->getEffectManager()->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                    EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                    pEffectAftermath->setDeadline(5 * 600); // lasts 5 minutes.
                } else {
                    EffectAftermath* pEffectAftermath = new EffectAftermath(pSlayer);
                    pEffectAftermath->setDeadline(5 * 600); // lasts 5 minutes.
                    pSlayer->getEffectManager()->addEffect(pEffectAftermath);
                    pSlayer->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                    pEffectAftermath->create(pSlayer->getName());
                }

                // Curing a blood drain changes the stats.
                SLAYER_RECORD prev;
                pSlayer->getSlayerRecord(prev);
                pSlayer->initAllStat();
                pSlayer->addModifyInfo(prev, gcMI);
                pSlayer->sendRealWearingInfo();
            }
        } else if (pPC->isVampire()) {
            Assert(pVampire != NULL);
            pVampire->setSilverDamage(0);
            pVampire->setHP(pVampire->getHP(ATTR_MAX));
            hp = pVampire->getHP();
            gcMI.addShortData(MODIFY_CURRENT_HP, hp);
            gcMI.addShortData(MODIFY_SILVER_DAMAGE, 0);
        } else if (pPC->isOusters()) {
            Assert(pOusters != NULL);
            pOusters->setSilverDamage(0);
            pOusters->setHP(pOusters->getHP(ATTR_MAX));
            hp = pOusters->getHP();
            gcMI.addShortData(MODIFY_CURRENT_HP, hp);
        } else {
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        if (GDRLairManager::Instance().isGDRLairZone(pPC->getZoneID())) {
            filelog("GDRLair.log", "%s used an elixir scroll in zone %d.", pPC->getName().c_str(), pPC->getZoneID());
        }
    } break;

    default:
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    pPC->deleteEffect(Effect::EFFECT_CLASS_COMA);
    pPC->removeFlag(Effect::EFFECT_CLASS_COMA);

    gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_COMA);
    pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcRemoveEffect);

    pPC->getEffectManager()->sendEffectInfo(pPC, pZone, pPC->getX(), pPC->getY());

    GCStatusCurrentHP gcHP;
    gcHP.setObjectID(pPC->getObjectID());
    gcHP.setCurrentHP(hp);
    pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcHP, pPC);

    pPlayer->sendPacket(&gcMI);

    GCUseOK gcUseOK;
    pPlayer->sendPacket(&gcUseOK);

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}


void CGUseItemFromInventoryHandler::executeTranslator(CGUseItemFromInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());

    if (pItem->getObjectID() != pPacket->getObjectID() || pItemInfo == NULL ||
        pPC->isFlag(Effect::EFFECT_CLASS_TRANSLATION)) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    EffectTranslation* pEffect = new EffectTranslation(pPC);
    Assert(pEffect != NULL);

    pEffect->setDeadline(6000); // 10 minutes
    pPC->addEffect(pEffect);
    pPC->setFlag(Effect::EFFECT_CLASS_TRANSLATION);

    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pPC->getObjectID());
    gcAddEffect.setEffectID(pEffect->getSendEffectClass());
    gcAddEffect.setDuration(6000);
    pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddEffect);

    GCUseOK gcUseOK;
    pPlayer->sendPacket(&gcUseOK);

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

void CGUseItemFromInventoryHandler::executeEffectItem(CGUseItemFromInventory* pPacket, Player* pPlayer) {
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);
    EffectItemInfo* pItemInfo = dynamic_cast<EffectItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));

    if (pItem->getObjectID() != pPacket->getObjectID() || pItemInfo == NULL ||
        pPC->isFlag(pItemInfo->getEffectClass())) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    SimpleCreatureEffect* pEffect = new SimpleCreatureEffect(pItemInfo->getEffectClass(), pPC);
    Assert(pEffect != NULL);

    pEffect->setDeadline((WORD)pItemInfo->getDuration() * 10);
    pPC->addEffect(pEffect);
    pPC->setFlag(pEffect->getEffectClass());

    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pPC->getObjectID());
    gcAddEffect.setEffectID(pEffect->getSendEffectClass());
    gcAddEffect.setDuration((WORD)pItemInfo->getDuration() * 10);
    pZone->broadcastPacket(pPC->getX(), pPC->getY(), &gcAddEffect);

    GCUseOK gcUseOK;
    pPlayer->sendPacket(&gcUseOK);

    if (isStackable(pItem)) {
        decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
    } else {
        // A non-stacking item is deleted right away.
        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

void CGUseItemFromInventoryHandler::executePetItem(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__
    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    if (pPC == NULL) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    if (pPC != NULL && pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        if (pSlayer->hasRideMotorcycle()) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }
    }

    if (pPC != NULL &&
        (pPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) || pPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
         pPC->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH))) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    PetItem* pPetItem = dynamic_cast<PetItem*>(pItem);
    if (pPetItem != NULL) {
        PetInfo* pTargetPetInfo = pPetItem->getPetInfo();
        if (pTargetPetInfo->getPetHP() == 0) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        if (pTargetPetInfo->getPetType() >= PET_CENTAURO && pPC->getQuestLevel() < 40) {
            filelog("Pet.log", "A player below the required level tried to summon a second-stage pet : [%s:%s]",
                    pGamePlayer->getID().c_str(), pPC->getName().c_str());
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        PetInfo* pPetInfo = pPC->getPetInfo();
        if (pPetInfo == NULL || pPetInfo->getPetItem() != pPetItem) {
            pPC->setPetInfo(pTargetPetInfo);
            cout << pPetItem->getObjectID() << " call the pet item." << endl;
        } else {
            cout << "recall the pet" << endl;
            pPC->setPetInfo(NULL);
        }

        pPC->initAllStatAndSend();
        sendPetInfo(pGamePlayer, true, true);
        GCUseOK gcUseOK;
        pGamePlayer->sendPacket(&gcUseOK);
    }
#endif

    __END_CATCH
}

void CGUseItemFromInventoryHandler::executePetFood(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY

#ifdef __GAME_SERVER__
    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    PetFood* pPetFood = dynamic_cast<PetFood*>(pItem);
    PetFoodInfo* pInfo = dynamic_cast<PetFoodInfo*>(
        de::gameContext().itemInfos().getItemInfo(pPetFood->getItemClass(), pPetFood->getItemType()));

    if (pPetFood != NULL && pInfo != NULL) {
        PetInfo* pPetInfo = pPC->getPetInfo();
        if (pPetInfo == NULL || pPetInfo->getPetHP() == 0) {
            sendCannotUse(pPacket, pPlayer);
            return;
        }

        PetTypeInfo* pPetTypeInfo = PetTypeInfoManager::getInstance()->getPetTypeInfo(pPetInfo->getPetType());

        if (pPetTypeInfo == NULL || pPetTypeInfo->getFoodType() != pInfo->getTarget()) {
            sendCannotUse(pPacket, pPlayer);
            return;
        } else {
            pPetInfo->setPetHP(pInfo->getPetHP());
            pPetInfo->setFoodType(pPetFood->getItemType());

            decreaseItemNum(pItem, pInventory, pCreature->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);


            sendPetInfo(pGamePlayer, true);

            GCUseOK gcUseOK;
            pGamePlayer->sendPacket(&gcUseOK);

            char query[100];
            snprintf(query, sizeof(query), "PetHP=%u, FoodType=%u", pPetInfo->getPetHP(), pPetInfo->getFoodType());

            Item* pItem = pPetInfo->getPetItem();
            if (pItem != NULL)
                pItem->tinysave(query);
        }
    }
#endif

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUseItemFromInventoryHandler::executeEventGiftBox(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Zone* pZone = pPC->getZone();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    // It cannot be used unless it is a black gift box
    if (pItem->getItemType() < 6 || (pItem->getItemType() >= 16 && pItem->getItemType() <= 18)) {
        filelog("GiftBoxErrorLog.txt", "[Name] : %s , [ItemType] : %d : Invalid item type\n",
                pCreature->getName().c_str(), pItem->getItemType());
        return;
    }

    // DEBUG
    cout << "Name : " << pCreature->getName() << " , GiftBoxType : " << pItem->getItemType() << endl;

    if (pItem->getItemType() >= 22 && pItem->getItemType() <= 26) {
        sendCannotUse(pPacket, pPlayer);
        return;
    }

    // What the black box turns into always fits in 2*2, so the Inventory check is skipped
    // (a situation where it does not fit after the black box is erased would be awkward)
    ItemMineInfoManager& itemMineInfos = de::gameContext().itemMineInfos();
    ItemFactoryManager& itemFactories = de::gameContext().itemFactories();

    Item* pResultItem = NULL;
    bool bFullStack = true;

    if (pItem->getItemType() == 6) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(113, 122);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(123, 132);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(133, 142);
        }
    } else if (pItem->getItemType() == 7) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(143, 152);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(153, 162);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(163, 172);
        }
    } else if (pItem->getItemType() == 8) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(173, 182);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(183, 192);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(193, 202);
        }
    } else if (pItem->getItemType() == 9) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(203, 212);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(213, 222);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(223, 232);
        }
    } else if (pItem->getItemType() == 10) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(233, 242);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(243, 252);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(253, 262);
        }
    } else if (pItem->getItemType() == 11) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(263, 272);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(273, 282);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(283, 292);
        }
    } else if (pItem->getItemType() == 12) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(293, 302);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(303, 312);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(313, 322);
        }
    } else if (pItem->getItemType() == 13) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(323, 332);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(333, 342);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(343, 352);
        }
    } else if (pItem->getItemType() == 14) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(353, 362);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(363, 372);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(373, 382);
        }
    } else if (pItem->getItemType() == 15) {
        if (pCreature->isSlayer()) {
            pResultItem = itemMineInfos.getRandomItem(383, 392);
        } else if (pCreature->isVampire()) {
            pResultItem = itemMineInfos.getRandomItem(393, 402);
        } else if (pCreature->isOusters()) {
            pResultItem = itemMineInfos.getRandomItem(403, 412);
        }
    } else if (pItem->getItemType() == 19) {
        int value = rand() % 100;
        bFullStack = false;

        if (value < 70) {
            // Elixir scroll
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_RESURRECT_ITEM, 1, list<OptionType_t>());
        } else if (value < 95) {
            // Red rice-cake soup
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_STAR, 8, list<OptionType_t>());
        } else if (value < 99) {
            // Bundle of elixir scrolls
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_RESURRECT_ITEM, 1, list<OptionType_t>());
            pResultItem->setNum(9);
        } else {
            // Accessory mixing forge, type A
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_MIXING_ITEM, 6, list<OptionType_t>());
        }
    } else if (pItem->getItemType() == 20) {
        int value = rand() % 100;
        bFullStack = false;

        if (value < 70) {
            // Resurrection scroll
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_RESURRECT_ITEM, 0, list<OptionType_t>());
        } else if (value < 95) {
            // Blue rice-cake soup
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_STAR, 10, list<OptionType_t>());
        } else if (value < 99) {
            // 50 bluebirds
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_STAR, 12, list<OptionType_t>());
            pResultItem->setNum(50);
        } else {
            // Armor mixing forge, type A
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_MIXING_ITEM, 3, list<OptionType_t>());
        }
    }

    else if (pItem->getItemType() == 21) {
        int value = rand() % 100;
        bFullStack = false;

        if (value < 70) {
            // Resurrection scroll
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_RESURRECT_ITEM, 0, list<OptionType_t>());
        } else if (value < 95) {
            // Green rice-cake soup
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_STAR, 9, list<OptionType_t>());
        } else if (value < 99) {
            // Notice board 3
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_EVENT_TREE, 28, list<OptionType_t>());
        } else {
            // 4 pet foods
            pResultItem = itemFactories.createItem(Item::ITEM_CLASS_PET_FOOD, 4, list<OptionType_t>());
            pResultItem->setNum(4);
        }
    }

    if (pResultItem == NULL) {
        filelog("GiftBoxErrorLog.txt", "[Name] : %s : No item to give\n", pCreature->getName().c_str());
        return;
    }

    // A stackable item is filled to the top
    if (pResultItem->isStackable() && bFullStack) {
        int MaxStack = ItemMaxStack[pResultItem->getItemClass()];
        pResultItem->setNum(MaxStack);
    }

    // A Charging item is filled up too
    if (pResultItem->getItemClass() == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM) {
        SlayerPortalItem* pSlayerPortalItem = dynamic_cast<SlayerPortalItem*>(pResultItem);
        pSlayerPortalItem->setCharge(pSlayerPortalItem->getMaxCharge());
    } else if (pResultItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM) {
        VampirePortalItem* pVampirePortalItem = dynamic_cast<VampirePortalItem*>(pResultItem);
        pVampirePortalItem->setCharge(pVampirePortalItem->getMaxCharge());
    } else if (pResultItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM) {
        OustersSummonItem* pOustersSummonItem = dynamic_cast<OustersSummonItem*>(pResultItem);
        pOustersSummonItem->setCharge(pOustersSummonItem->getMaxCharge());
    }

    pZone->registerObject(pResultItem);

    GCUseOK gcUseOK;
    pGamePlayer->sendPacket(&gcUseOK);
    // Erase the black gift box and put the item in its place
    pInventory->deleteItem(InvenX, InvenY);
    pItem->destroy();
    SAFE_DELETE(pItem);

    // Put it into the inventory.
    if (pInventory->addItem(InvenX, InvenY, pResultItem)) {
        pResultItem->create(pPC->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);

        GCCreateItem gcCreateItem;
        makeGCCreateItem(&gcCreateItem, pResultItem, InvenX, InvenY);

        pGamePlayer->sendPacket(&gcCreateItem);

        // Leave an ItemTraceLog
        if (pResultItem != NULL && pResultItem->isTraceItem()) {
            remainTraceLog(pResultItem, "BLACK BOX", pCreature->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
        }
    } else {
        filelog("GiftBoxErrorLog.txt", "[Name] : %s : Cannot put the item in the inventory. Item : %s\n",
                pCreature->getName().c_str(), pResultItem->toString().c_str());
        return;
    }


#endif

    __END_DEBUG_EX __END_CATCH
}

void CGUseItemFromInventoryHandler::executeTrapItem(CGUseItemFromInventory* pPacket, Player* pPlayer)

{
#ifdef __GAME_SERVER__
    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    // The enclosing function checked plenty of errors, so
    // the error checking here is cut right down.
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    CoordInven_t InvenX = pPacket->getX();
    CoordInven_t InvenY = pPacket->getY();
    Item* pItem = pInventory->getItem(InvenX, InvenY);

    TrapItem* pTrapItem = dynamic_cast<TrapItem*>(pItem);
    TrapItemInfo* pInfo = dynamic_cast<TrapItemInfo*>(
        de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));

    if (!SiegeManager::Instance().isSiegeZone(pPC->getZoneID())) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    if (pTrapItem != NULL && pInfo != NULL) {
        if (pTrapItem->getItemType() <= 3) {
            // Trap
            if ((pCreature->getX() < 97 || pCreature->getX() > 121) ||
                (pCreature->getY() < 135 || pCreature->getY() > 170)) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }
        } else {
            // Obstacle
            if ((pCreature->getX() < 132 || pCreature->getX() > 152) ||
                (pCreature->getY() < 105 || pCreature->getY() > 135)) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }
        }

        switch (pInfo->getFunction()) {
        case TrapItemInfo::SUMMON_MONSTER:
        case TrapItemInfo::SUMMON_TRAP: {
            Monster* pMonster = new Monster(pInfo->getParameter());
            pCreature->getZone()->addCreature(pMonster, pCreature->getX(), pCreature->getY(), 2);
            if (pInfo->getFunction() == TrapItemInfo::SUMMON_TRAP)
                addSimpleCreatureEffect(pMonster, Effect::EFFECT_CLASS_HIDE_TO_ATTACKER);
        } break;
        case TrapItemInfo::MAKE_EFFECT: {
            Zone* pZone = pPC->getZone();

            ZoneCoord_t sx = pPC->getX();
            ZoneCoord_t sy = pPC->getY();

            Monster* pMonster = new Monster(pInfo->getParameter());
            pMonster->setBrain(NULL);
            EffectKillTimer* pTimer = new EffectKillTimer(pMonster);
            pTimer->setDeadline(65000);
            pMonster->setFlag(Effect::EFFECT_CLASS_NO_DAMAGE);
            pMonster->addEffect(pTimer);

            pCreature->getZone()->addCreature(pMonster, sx, sy, 2);
            addSimpleCreatureEffect(pMonster, Effect::EFFECT_CLASS_HIDE_TO_ATTACKER);
            sx = pMonster->getX();
            sy = pMonster->getY();

            GCAddEffectToTile gcAE;
            gcAE.setDuration(65000);
            gcAE.setXY(sx, sy);

            for (int i = 1; i <= 7; ++i) {
                ZoneCoord_t tx = sx + i;
                ZoneCoord_t ty = sy + i;
                if (!isValidZoneCoord(pZone, tx, ty))
                    continue;
                if (!pZone->getTile(tx, ty).canAddEffect())
                    continue;

                EffectTurretLaser* pTurretLaser = new EffectTurretLaser(pZone, tx, ty);
                pTurretLaser->setDeadline(65000);
                pTurretLaser->setNextTime(0);
                pZone->registerObject(pTurretLaser);
                pZone->addEffect(pTurretLaser);
                pZone->getTile(tx, ty).addEffect(pTurretLaser);

                gcAE.setEffectID(pTurretLaser->getSendEffectClass());
                gcAE.setObjectID(pTurretLaser->getObjectID());
                gcAE.setXY(tx, ty);
                pZone->broadcastPacket(tx, ty, &gcAE);
            }
        } break;
        }

        GCUseOK _GCUseOK;
        pGamePlayer->sendPacket(&_GCUseOK);

        pInventory->deleteItem(InvenX, InvenY);
        pItem->destroy();
        SAFE_DELETE(pItem);
    }

#endif
}
