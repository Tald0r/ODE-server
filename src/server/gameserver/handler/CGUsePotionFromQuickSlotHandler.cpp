//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUsePotionFromQuickSlotHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGUsePotionFromQuickSlot.h"

#ifdef __GAME_SERVER__
#include <math.h>

#include "CGRideMotorCycle.h"
#include "EffectHPRecovery.h"
#include "EffectMPRecovery.h"
#include "EffectRecallMotorcycle.h"
#include "EffectTransportItem.h"
#include "GCCannotUse.h"
#include "GCHPRecoveryStartToOthers.h"
#include "GCHPRecoveryStartToSelf.h"
#include "GCMPRecoveryStart.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GCUseOK.h"
#include "GDRLairManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "ParkingCenter.h"
#include "SiegeManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ctf/FlagManager.h"
#include "item/Belt.h"
#include "item/ComposMei.h"
#include "item/EventETC.h"
#include "item/Key.h"
#include "item/OustersArmsband.h"
#include "item/Potion.h"
#include "item/Pupa.h"
#include "repository/ItemObjectRepository.h"

bool UseYellowCandy(PlayerCreature* pPC, Item* pItem) {
    if (pPC->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;
    if (pItem->getItemClass() == Item::ITEM_CLASS_EVENT_ETC) {
        if (pItem->getItemType() >= 14) {
            EventETCInfo* pInfo = dynamic_cast<EventETCInfo*>(
                de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType()));
            Assert(pInfo != NULL);

            int amount = pInfo->getFunction();

            Zone* pZone = pPC->getZone();
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
                return false;
            }

            return true;
        } else {
            return false;
        }
    }

    return false;
}

#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGUsePotionFromQuickSlotHandler::execute(CGUsePotionFromQuickSlot* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    ParkingCenter& parking = de::gameContext().parking();

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();

    // It cannot be used in the coma state.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
        GCCannotUse _GCCannotUse;
        _GCCannotUse.setObjectID(pPacket->getObjectID());
        pGamePlayer->sendPacket(&_GCCannotUse);
        return;
    }

    try {
        if (pCreature->isSlayer()) {
            GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
            Creature* pCreature = pGamePlayer->getCreature();

            if (!pCreature->isSlayer())
                return;

            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Zone* pZone = pSlayer->getZone();
            Item* pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);


            if (pBelt == NULL) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the belt's inventory.
            Inventory* pBeltInventory = ((Belt*)pBelt)->getInventory();

            // Get the SlotID.
            SlotID_t SlotID = pPacket->getSlotID();

            // It cannot be used past the slot range.
            if (SlotID >= pBeltInventory->getWidth()) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the item in the belt's given slot.
            Item* pBeltItem = pBeltInventory->getItem(SlotID, 0);

            // With no item there is of course nothing to use.
            if (pBeltItem == NULL) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the Object of the item in the slot.
            ObjectID_t ItemObjectID = pBeltItem->getObjectID();

            // It cannot be used when the data does not match or it is not a potion.
            if (ItemObjectID != pPacket->getObjectID() || (pBeltItem->getItemClass() != Item::ITEM_CLASS_POTION &&
                                                           pBeltItem->getItemClass() != Item::ITEM_CLASS_KEY)) {
                if (ItemObjectID != pPacket->getObjectID()) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                } else if (UseYellowCandy(pSlayer, pBeltItem)) {
                    GCUseOK _GCUseOK;
                    pGamePlayer->sendPacket(&_GCUseOK);
                    decreaseItemNum(pBeltItem, pBeltInventory, pSlayer->getName(), STORAGE_BELT, pBelt->getItemID(),
                                    SlotID, 0);
                } else {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                }

                return;
            }


            if (pBeltItem->getItemClass() == Item::ITEM_CLASS_KEY) {
                if (!de::gameContext().variables().isSummonMotorcycle() || pSlayer->hasRideMotorcycle() ||
                    pBeltItem->getItemClass() != Item::ITEM_CLASS_KEY || pSlayer->isFlag(Effect::EFFECT_CLASS_COMA) ||
                    (pZone->getZoneLevel(pCreature->getX(), pCreature->getY()) & SAFE_ZONE) || pZone->isMasterLair() ||
                    pZone->isNoPortalZone() || (!pGamePlayer->isPremiumPlay() && !pGamePlayer->isPayPlaying()) ||
                    de::gameContext().flags().isInPoleField(
                        ZONE_COORD(pZone->getZoneID(), pCreature->getX(), pCreature->getY())) ||
                    GDRLairManager::Instance().isGDRLairZone(pZone->getZoneID())) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                    return;
                }

                if (SiegeManager::Instance().isSiegeZone(pSlayer->getZoneID())) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                    return;
                }

                // Check whether the item already exists.
                ItemID_t targetID = dynamic_cast<Key*>(pBeltItem)->getTarget();

                if (targetID == 0) {
                    Key* pKey = dynamic_cast<Key*>(pBeltItem);
                    Assert(pKey != NULL);

                    targetID = pKey->setNewMotorcycle(pSlayer);
                } else {
                    // Once a motorcycle and a key are linked, someone keeps deleting the motorcycle.
                    // Check that the motorcycle linked to the key really is in the DB, and if not make a new one.
                    if (!defaultItemObjectRepository().motorcycleExists(targetID)) {
                        Key* pKey = dynamic_cast<Key*>(pBeltItem);
                        Assert(pKey != NULL);

                        targetID = pKey->setNewMotorcycle(pSlayer);
                    }
                }
                // Last-ditch defensive code
                if (targetID == 0) {
                    filelog("errorLog.txt", "[ActionRedeemMotorcycle] itemID=%lu, motorItemID=%lu",
                            (int)pBeltItem->getItemID(), (int)targetID);
                    return;
                }


                if (parking.hasMotorcycleBox(targetID)) {
                    cout << "A motorcycle is already summoned" << endl;

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
                            EffectRecallMotorcycle* pEffectRecallMotorcycle =
                                new EffectRecallMotorcycle(pMotorZone, motorX, motorY, pZone, pSlayer->getX(),
                                                           pSlayer->getY(), pMotorcycle, pSlayer->getObjectID(), 0);
                            pMotorZone->registerObject(pEffectRecallMotorcycle);
                            pMotorZone->addEffect_LOCKING(pEffectRecallMotorcycle);

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
                    bool bFound = defaultItemObjectRepository().loadMotorcycleForRedeem(REDEEM_SPELLING_HANDLER,
                                                                                        targetID, redeemRow);

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
                    cout << "Adding the motorcycle to the zone" << pSlayer->getX() << " " << pSlayer->getY() << endl;
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

                        // It burns out.
                        CGRideMotorCycle cgRide;
                        cgRide.setObjectID(pMotorcycle->getObjectID());
                        cgRide.setX(pt.x);
                        cgRide.setY(pt.y);
                        // synthetic packet straight to the handler (2.3)
                        CGRideMotorCycleHandler::execute(&cgRide, pGamePlayer);
                    }


                    // This is the verify packet.
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                }

                return;
            }

            HP_t MaxHP = pSlayer->getHP(ATTR_MAX);
            HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
            MP_t MaxMP = pSlayer->getMP(ATTR_MAX);
            MP_t CurrentMP = pSlayer->getMP(ATTR_CURRENT);
            Potion* pPotion = dynamic_cast<Potion*>(pBeltItem);

            // the amount recovered per turn
            int HPQuantity = pPotion->getHPQuantity();
            int MPQuantity = pPotion->getMPQuantity();

            // how many seconds one turn is.
            int HPDelayProvider = pPotion->getHPDelay();
            int MPDelayProvider = pPotion->getMPDelay();

            Attr_t INT = pSlayer->getINT();

            HP_t PotionHPAmount = 0;

            PotionHPAmount = pPotion->getHPAmount();

            int HPAmount = min(MaxHP - CurrentHP, (int)PotionHPAmount);
            int MPAmount = min(MaxMP - CurrentMP, (int)(pPotion->getMPAmount() * (1 + (double)((double)INT / 300.0))));
            bool notRecoverHP = false;
            bool notRecoverMP = false;

            // With the Activation Effect on, the recovery rate doubles.
            if (pSlayer->isFlag(Effect::EFFECT_CLASS_ACTIVATION)) {
                if (pPotion->getItemType() >= 14 && pPotion->getItemType() <= 17) {
                } else {
                    HPDelayProvider = (HPDelayProvider >> 1);
                    MPDelayProvider = (MPDelayProvider >> 1);

                    HPDelayProvider = max(HPDelayProvider, 1);
                    MPDelayProvider = max(MPDelayProvider, 1);
                }
            }


            // If there is an HP recovery amount...
            if (HPAmount != 0 && HPQuantity != 0) {
                if (CurrentHP < MaxHP) {
                    EffectManager* pEffectManager = pSlayer->getEffectManager();

                    double temp = (double)((double)HPAmount / (double)HPQuantity);
                    uint Period = (uint)ceil(temp);
                    Turn_t Deadline = Period * HPDelayProvider;

                    if (pSlayer->isFlag(Effect::EFFECT_CLASS_HP_RECOVERY)) {
                        Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_HP_RECOVERY);
                        EffectHPRecovery* pEffectHPRecoveryEffect = dynamic_cast<EffectHPRecovery*>(pEffect);

                        // Compute the HP the existing unit amount and count would fill.
                        // Add that to the current recovery amount.
                        int PrevHPAmount =
                            pEffectHPRecoveryEffect->getHPQuantity() * pEffectHPRecoveryEffect->getPeriod();
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
                // How much, how many times, every how many seconds.
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
                        int PrevMPAmount =
                            pEffectMPRecoveryEffect->getMPQuantity() * pEffectMPRecoveryEffect->getPeriod();
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
            } else {
                decreaseItemNum(pBeltItem, pBeltInventory, pSlayer->getName(), STORAGE_BELT, pBelt->getItemID(), SlotID,
                                0);
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            Zone* pZone = pOusters->getZone();

            // Get the SlotID.
            SlotID_t SlotID = pPacket->getSlotID();

            Ousters::WearPart part = (SlotID > 2 ? Ousters::WEAR_ARMSBAND2 : Ousters::WEAR_ARMSBAND1);
            if (SlotID > 2)
                SlotID -= 3;

            Item* pOustersArmsband = pOusters->getWearItem(part);

            if (pOustersArmsband == NULL) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the belt's inventory.
            Inventory* pOustersArmsbandInventory = ((OustersArmsband*)pOustersArmsband)->getInventory();

            // It cannot be used past the slot range.
            if (SlotID >= pOustersArmsbandInventory->getWidth()) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the item in the belt's given slot.
            Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem(SlotID, 0);

            // With no item there is of course nothing to use.
            if (pOustersArmsbandItem == NULL) {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
                return;
            }

            // Get the Object of the item in the slot.
            ObjectID_t ItemObjectID = pOustersArmsbandItem->getObjectID();

            // It cannot be used when the data does not match or it is neither a pupa nor a compos mei.
            if (ItemObjectID != pPacket->getObjectID() ||
                (pOustersArmsbandItem->getItemClass() != Item::ITEM_CLASS_PUPA &&
                 pOustersArmsbandItem->getItemClass() != Item::ITEM_CLASS_COMPOS_MEI)) {
                if (ItemObjectID != pPacket->getObjectID()) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                } else if (UseYellowCandy(pOusters, pOustersArmsbandItem)) {
                    GCUseOK _GCUseOK;
                    pGamePlayer->sendPacket(&_GCUseOK);
                    decreaseItemNum(pOustersArmsbandItem, pOustersArmsbandInventory, pOusters->getName(), STORAGE_BELT,
                                    pOustersArmsband->getItemID(), SlotID, 0);
                } else {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pGamePlayer->sendPacket(&_GCCannotUse);
                }

                return;
            }

            bool regenHP = false;
            bool regenMP = false;

            HP_t MaxHP = 0;
            HP_t CurrentHP = 0;
            int HPQuantity = 0;
            int HPDelayProvider = 0;
            HP_t PupaHPAmount = 0;
            int HPAmount = 0;

            if (pOustersArmsbandItem->getItemClass() == Item::ITEM_CLASS_PUPA) {
                MaxHP = pOusters->getHP(ATTR_MAX);
                CurrentHP = pOusters->getHP(ATTR_CURRENT);

                Pupa* pPupa = dynamic_cast<Pupa*>(pOustersArmsbandItem);

                // the amount recovered per turn
                HPQuantity = pPupa->getHPQuantity();

                // how many seconds one turn is.
                HPDelayProvider = pPupa->getHPDelay();

                PupaHPAmount = pPupa->getHPAmount();

                HPAmount = min(MaxHP - CurrentHP, (int)PupaHPAmount);
            } else if (pOustersArmsbandItem->getItemClass() == Item::ITEM_CLASS_COMPOS_MEI) {
                MaxHP = pOusters->getHP(ATTR_MAX);
                CurrentHP = pOusters->getHP(ATTR_CURRENT);

                ComposMei* pComposMei = dynamic_cast<ComposMei*>(pOustersArmsbandItem);

                // the amount recovered per turn
                HPQuantity = pComposMei->getHPQuantity();

                // how many seconds one turn is.
                HPDelayProvider = pComposMei->getHPDelay();

                PupaHPAmount = pComposMei->getHPAmount();

                HPAmount = min(MaxHP - CurrentHP, (int)PupaHPAmount);
            }

            // If there is an HP recovery amount...
            if (HPAmount != 0 && HPQuantity != 0) {
                if (CurrentHP < MaxHP) {
                    EffectManager* pEffectManager = pOusters->getEffectManager();

                    double temp = (double)((double)HPAmount / (double)HPQuantity);
                    int Period = (uint)ceil(temp);
                    Turn_t Deadline = Period * HPDelayProvider;

                    if (pOusters->isFlag(Effect::EFFECT_CLASS_HP_RECOVERY)) {
                        Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_HP_RECOVERY);
                        EffectHPRecovery* pEffectHPRecoveryEffect = dynamic_cast<EffectHPRecovery*>(pEffect);

                        // Compute the HP the existing unit amount and count would fill.
                        // Add that to the current recovery amount.
                        int PrevHPAmount =
                            pEffectHPRecoveryEffect->getHPQuantity() * pEffectHPRecoveryEffect->getPeriod();
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
                        gcHPRecoveryStartToOthers.setObjectID(pOusters->getObjectID());
                        gcHPRecoveryStartToOthers.setPeriod(pEffectHPRecoveryEffect->getPeriod());
                        gcHPRecoveryStartToOthers.setDelay(pEffectHPRecoveryEffect->getDelay());
                        gcHPRecoveryStartToOthers.setQuantity(pEffectHPRecoveryEffect->getHPQuantity());

                        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcHPRecoveryStartToOthers,
                                               pOusters);
                        GCUseOK _GCUseOK;
                        pGamePlayer->sendPacket(&_GCUseOK);
                    } else {
                        EffectHPRecovery* pEffectHPRecovery = new EffectHPRecovery();

                        pEffectHPRecovery->setTarget(pOusters);
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
                        gcHPRecoveryStartToOthers.setObjectID(pOusters->getObjectID());
                        gcHPRecoveryStartToOthers.setPeriod(Period);
                        gcHPRecoveryStartToOthers.setDelay(HPDelayProvider);
                        gcHPRecoveryStartToOthers.setQuantity(HPQuantity);

                        pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &gcHPRecoveryStartToOthers,
                                               pOusters);
                        GCUseOK _GCUseOK;
                        pGamePlayer->sendPacket(&_GCUseOK);
                    }

                    regenHP = true;
                }
                //			decreaseItemNum(pOustersArmsbandItem, pInventory, pOusters->getName(), STORAGE_INVENTORY, 0,
                // InvenX, InvenY);
            }

            MP_t MaxMP = 0;
            MP_t CurrentMP = 0;

            int MPQuantity = 0;
            int MPDelayProvider = 0;
            MP_t ComposMeiMPAmount = 0;
            int MPAmount = 0;

            if (pOustersArmsbandItem->getItemClass() == Item::ITEM_CLASS_COMPOS_MEI) {
                MaxMP = pOusters->getMP(ATTR_MAX);
                CurrentMP = pOusters->getMP(ATTR_CURRENT);
                ComposMei* pComposMei = dynamic_cast<ComposMei*>(pOustersArmsbandItem);

                // the amount recovered per turn
                MPQuantity = pComposMei->getMPQuantity();

                // how many seconds one turn is.
                MPDelayProvider = pComposMei->getMPDelay();

                Attr_t INT = pOusters->getINT();

                ComposMeiMPAmount = pComposMei->getMPAmount();

                MPAmount =
                    min(MaxMP - CurrentMP, (int)(ComposMeiMPAmount * (double)(1 + (double)((double)INT / 300.0))));
            } else if (pOustersArmsbandItem->getItemClass() == Item::ITEM_CLASS_PUPA) {
                MaxMP = pOusters->getMP(ATTR_MAX);
                CurrentMP = pOusters->getMP(ATTR_CURRENT);
                Pupa* pPupa = dynamic_cast<Pupa*>(pOustersArmsbandItem);

                // the amount recovered per turn
                MPQuantity = pPupa->getMPQuantity();

                // how many seconds one turn is.
                MPDelayProvider = pPupa->getMPDelay();

                Attr_t INT = pOusters->getINT();

                ComposMeiMPAmount = pPupa->getMPAmount();

                MPAmount =
                    min(MaxMP - CurrentMP, (int)(ComposMeiMPAmount * (double)(1 + (double)((double)INT / 300.0))));
            }

            // If there is an MP recovery amount...
            if (MPAmount != 0 && MPQuantity != 0) {
                if (CurrentMP < MaxMP) {
                    EffectManager* pEffectManager = pOusters->getEffectManager();

                    double temp = (double)((double)MPAmount / (double)MPQuantity);
                    uint Period = (uint)ceil(temp);
                    Turn_t Deadline = Period * MPDelayProvider;

                    if (pOusters->isFlag(Effect::EFFECT_CLASS_MP_RECOVERY)) {
                        Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_MP_RECOVERY);
                        EffectMPRecovery* pEffectMPRecoveryEffect = dynamic_cast<EffectMPRecovery*>(pEffect);

                        // Compute the MP the existing unit amount and count would fill.
                        // Add that to the current recovery amount.
                        int PrevMPAmount =
                            pEffectMPRecoveryEffect->getMPQuantity() * pEffectMPRecoveryEffect->getPeriod();
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

                        pEffectMPRecovery->setTarget(pOusters);
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

                    regenMP = true;
                }
                //			decreaseItemNum(pOustersArmsbandItem, pInventory, pOusters->getName(), STORAGE_INVENTORY, 0,
                // InvenX, InvenY);
            }

            if (regenMP || regenHP) {
                decreaseItemNum(pOustersArmsbandItem, pOustersArmsbandInventory, pOusters->getName(), STORAGE_BELT,
                                pOustersArmsband->getItemID(), SlotID, 0);
                //				decreaseItemNum(pOustersArmsbandItem, pInventory, pOusters->getName(),
                // STORAGE_INVENTORY, 0, InvenX, InvenY);
            } else {
                GCCannotUse _GCCannotUse;
                _GCCannotUse.setObjectID(pPacket->getObjectID());
                pGamePlayer->sendPacket(&_GCCannotUse);
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
