//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestRepairHandler.cpp
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRequestRepair.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "NPC.h"
#include "Ousters.h"
#include "PriceManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"
#include "item/Key.h"
#include "item/OustersSummonItem.h"
#include "item/SlayerPortalItem.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRequestRepairHandler::execute(CGRequestRepair* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    ObjectID_t ITEMOID = pPacket->getObjectID();
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pPC = pGamePlayer->getCreature();
    bool bSlayer = false;
    bool bVampire = false;
    bool bOusters = false;
    Item* pItem = NULL;

    // Tell whether the player is a Slayer or a Vampire.
    if (pPC->isSlayer())
        bSlayer = true;
    else if (pPC->isVampire())
        bVampire = true;
    else if (pPC->isOusters())
        bOusters = true;
    else
        throw ProtocolException("CGRequestRepairHandler::execute() : Unknown player creature!");

    if (ITEMOID == 0) {
        // An ObjectID of 0 means every item is to be repaired.
        executeAll(pPacket, pPlayer);
    } else {
        if (bSlayer)
            pItem = (dynamic_cast<Slayer*>(pPC))->findItemOID(ITEMOID);
        else if (bVampire)
            pItem = (dynamic_cast<Vampire*>(pPC))->findItemOID(ITEMOID);
        else if (bOusters)
            pItem = (dynamic_cast<Ousters*>(pPC))->findItemOID(ITEMOID);

        // If the player holds the item
        if (pItem != NULL) {
            // If that item is a motorcycle key...
            if (pItem->getItemClass() == Item::ITEM_CLASS_KEY && pItem->getItemType() == 2) {
                executeMotorcycle(pPacket, pPlayer);
                return;
            } else
                executeNormal(pPacket, pPlayer);
        } else {
            // There is no item, so of course nothing can be repaired.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_REPAIR_FAIL_ITEM_NOT_EXIST);
            pPlayer->sendPacket(&response);
        }
    }

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Handles an ordinary item.
//////////////////////////////////////////////////////////////////////////////
void CGRequestRepairHandler::executeNormal(CGRequestRepair* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        ObjectID_t ITEMOID = pPacket->getObjectID();
    Creature* pPC = dynamic_cast<GamePlayer*>(pPlayer)->getCreature();
    bool bSlayer = false;
    bool bVampire = false;
    bool bOusters = false;
    Gold_t playerMoney = 0;
    Price_t repairPrice = 0;
    Item* pItem = NULL;
    Slayer* pSlayer = NULL;
    Vampire* pVampire = NULL;
    Ousters* pOusters = NULL;
    int storage = 0;
    int X = 0;
    int Y = 0;
    GCNPCResponse response;

    // Tell whether the player is a Slayer or a Vampire.
    if (pPC->isSlayer())
        bSlayer = true;
    else if (pPC->isVampire())
        bVampire = true;
    else if (pPC->isOusters())
        bOusters = true;

    // Check whether the player holds the item it wants to repair
    if (bSlayer) {
        pSlayer = dynamic_cast<Slayer*>(pPC);
        playerMoney = pSlayer->getGold();
        pItem = pSlayer->findItemOID(ITEMOID, storage, X, Y);
    } else if (bVampire) {
        pVampire = dynamic_cast<Vampire*>(pPC);
        playerMoney = pVampire->getGold();
        pItem = pVampire->findItemOID(ITEMOID, storage, X, Y);
    } else if (bOusters) {
        pOusters = dynamic_cast<Ousters*>(pPC);
        playerMoney = pOusters->getGold();
        pItem = pOusters->findItemOID(ITEMOID, storage, X, Y);
    }

    // Whether the player holds the item it wants to repair
    // is checked further up, so pItem cannot be null.
    // What is checked here is whether the item cannot be repaired.
    if (isRepairableItem(pItem) == false) {
        response.setCode(NPC_RESPONSE_REPAIR_FAIL_ITEM_TYPE);
        pPlayer->sendPacket(&response);
        return;
    }

    // Save the previous durability.
    Durability_t oldDurability = pItem->getDurability();

    repairPrice = de::gameContext().prices().getRepairPrice(pItem);

    if (repairPrice > playerMoney) {
        response.setCode(NPC_RESPONSE_REPAIR_FAIL_MONEY);
        pPlayer->sendPacket(&response);
        return;
    }

    // Repair it.
    repairItem(pItem);

    // A repaired item in the gear window whose previous durability was 0 needs its information sent again.
    if (storage == STORAGE_GEAR && oldDurability == 0) {
        if (bSlayer && pSlayer != NULL) {
            pSlayer->initAllStatAndSend();
            pSlayer->sendRealWearingInfo();
        } else if (bVampire && pVampire != NULL) {
            pVampire->initAllStatAndSend();
            pVampire->sendRealWearingInfo();
        } else if (bOusters && pOusters != NULL) {
            pOusters->initAllStatAndSend();
            pOusters->sendRealWearingInfo();
        }
    }

    // Take the money.
    if (bSlayer) {
        pSlayer->decreaseGoldEx(repairPrice);
    } else if (bVampire) {
        // by sigi. 2002.9.4
        pVampire->decreaseGoldEx(repairPrice);
    } else if (bOusters) {
        // by sigi. 2002.9.4
        pOusters->decreaseGoldEx(repairPrice);
    }

    // Save to the DB that the item was repaired.
    // STORAGE_STASH can certainly come back, but
    // repairing something in the stash makes no sense, so
    // it is not saved.


    if (repairPrice > 0) {
        char pField[80];

        if (pItem->getItemClass() == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM) {
            SlayerPortalItem* pSPItem = dynamic_cast<SlayerPortalItem*>(pItem);
            snprintf(pField, sizeof(pField), "Charge=%d", pSPItem->getCharge());
        } else if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM) {
            OustersSummonItem* pOSItem = dynamic_cast<OustersSummonItem*>(pItem);
            snprintf(pField, sizeof(pField), "Charge=%d", pOSItem->getCharge());
        } else {
            snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
        }

        pItem->tinysave(pField);
    }


    // Send the OK packet.
    response.setCode(NPC_RESPONSE_REPAIR_OK);
    response.setParameter(playerMoney - repairPrice);
    pPlayer->sendPacket(&response);

#endif

    __END_DEBUG_EX __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
// Handles a motorcycle.
//////////////////////////////////////////////////////////////////////////////
void CGRequestRepairHandler::executeMotorcycle(CGRequestRepair* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        // Pull the packet information out.
        ObjectID_t ITEMOID = pPacket->getObjectID();
    Creature* pPC = dynamic_cast<GamePlayer*>(pPlayer)->getCreature();
    Zone* pZone = pPC->getZone();
    Gold_t playerMoney = 0;
    ZoneCoord_t CenterX = pPC->getX();
    ZoneCoord_t CenterY = pPC->getY();
    Item* pItem = NULL;
    bool bSlayer = false;
    bool bVampire = false;
    bool bOusters = false;
    GCNPCResponse response;

    // Tell whether the player is a Slayer or a Vampire.
    if (pPC->isSlayer())
        bSlayer = true;
    else if (pPC->isVampire())
        bVampire = true;
    else if (pPC->isOusters())
        bOusters = true;
    else
        throw ProtocolException("CGRequestRepairHandler::execute() : Unknown player creature!");

    // Whether the player holds the item it wants to repair
    // is checked further up, so pItem cannot be null.
    if (bSlayer) {
        pItem = (dynamic_cast<Slayer*>(pPC))->findItemOID(ITEMOID);
        playerMoney = (dynamic_cast<Slayer*>(pPC))->getGold();
    } else if (bVampire) {
        pItem = (dynamic_cast<Vampire*>(pPC))->findItemOID(ITEMOID);
        playerMoney = (dynamic_cast<Vampire*>(pPC))->getGold();
    } else if (bOusters) {
        pItem = (dynamic_cast<Ousters*>(pPC))->findItemOID(ITEMOID);
        playerMoney = (dynamic_cast<Ousters*>(pPC))->getGold();
    }

    // Search a range around, to see whether a motorcycle is there.
    for (ZoneCoord_t zx = CenterX - 5; zx <= CenterX + 5; zx++) {
        for (ZoneCoord_t zy = CenterY - 5; zy <= CenterY + 5; zy++) {
            if (!isValidZoneCoord(pZone, zx, zy))
                continue;

            Tile& tile = pZone->getTile(zx, zy);

            if (tile.hasItem()) {
                Item* pItemOnTile = tile.getItem();
                Assert(pItemOnTile != NULL);

                // If an item sits on the tile, check whether it is a motorcycle.
                if (pItemOnTile->getItemClass() == Item::ITEM_CLASS_MOTORCYCLE) {
                    DWORD targetID = dynamic_cast<Key*>(pItem)->getTarget();
                    ItemID_t motorcycleID = pItemOnTile->getItemID();

                    if (targetID == motorcycleID) {
                        Price_t repairPrice = de::gameContext().prices().getRepairPrice(pItemOnTile);

                        if (repairPrice > playerMoney) {
                            response.setCode(NPC_RESPONSE_REPAIR_FAIL_MONEY);
                            pPlayer->sendPacket(&response);
                            return;
                        }

                        // Repair it.
                        repairItem(pItemOnTile);

                        // Save it.
                        char pField[80];
                        snprintf(pField, sizeof(pField), "Durability=%d", pItemOnTile->getDurability());
                        pItemOnTile->tinysave(pField);


                        // Take the money.

                        // by sigi. 2002.9.4
                        (dynamic_cast<PlayerCreature*>(pPC))->decreaseGoldEx(repairPrice);

                        response.setCode(NPC_RESPONSE_REPAIR_OK);
                        response.setParameter(playerMoney - repairPrice);
                        pPlayer->sendPacket(&response);

                        return;
                    } // if (targetID ==
                } // if (itemclas == MOTORCYCLE
            }
        } // end of for (ZoneCoord_t zy=CenterY-5; zy<=CenterY+5; zy++)
    } // end of for (ZoneCoord_t zx=CenterX-5; zx<=CenterX+5; zx++)

    // Coming this far through the FOR loop means there is no motorcycle nearby...
    // So report that selling the motorcycle failed.
    response.setCode(NPC_RESPONSE_REPAIR_FAIL_ITEM_NOT_EXIST);
    pPlayer->sendPacket(&response);

#endif

    __END_DEBUG_EX __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Repair every item
//////////////////////////////////////////////////////////////////////////////
void CGRequestRepairHandler::executeAll(CGRequestRepair* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Creature* pPC = dynamic_cast<GamePlayer*>(pPlayer)->getCreature();
    Price_t repairPrice = 0;
    GCNPCResponse response;

    bool bSendRealWearingInfo = false;

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);

        // Compute the repair price summed over every item.
        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            Item* pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL) {
                if (i == Slayer::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // For the right hand, when the weapon held is two-handed...
                    // it does not have to be counted in the repair price.
                } else {
                    repairPrice += de::gameContext().prices().getRepairPrice(pItem);
                }
            }
        }

        // Return if there is not enough money.
        if (pSlayer->getGold() < repairPrice) {
            response.setCode(NPC_RESPONSE_REPAIR_FAIL_MONEY);
            pPlayer->sendPacket(&response);
            return;
        }

        // Repair each item and save it to the DB.
        char pField[80];

        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            Item* pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL) {
                if (i == Slayer::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // For the right hand, when the weapon held is two-handed...
                    // No repair needed.
                } else if (isRepairableItem(pItem)) {
                    Durability_t oldDurability = pItem->getDurability();
                    repairItem(pItem);
                    if (pItem->getDurability() != oldDurability) {
                        // To cut down DB queries,
                        // save only when the durability changed.
                        snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
                        pItem->tinysave(pField);
                    }

                    if (oldDurability == 0)
                        bSendRealWearingInfo = true;
                }
            }
        }

        // Take the money, and...

        // by sigi.2002.9.4
        pSlayer->decreaseGoldEx(repairPrice);


        // Send the OK packet.
        response.setCode(NPC_RESPONSE_REPAIR_OK);
        response.setParameter(pSlayer->getGold());
        pPlayer->sendPacket(&response);
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);

        // Compute the repair price summed over every item.
        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            Item* pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL) {
                if (i == Vampire::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // A two-handed weapon is repaired on one side only.
                } else {
                    repairPrice += de::gameContext().prices().getRepairPrice(pItem);
                }
            }
        }

        // Return if there is not enough money.
        if (pVampire->getGold() < repairPrice) {
            response.setCode(NPC_RESPONSE_REPAIR_FAIL_MONEY);
            pPlayer->sendPacket(&response);
            return;
        }

        // Repair each item and save it to the DB.
        char pField[80];

        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            Item* pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL) {
                if (i == Vampire::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // A two-handed weapon is repaired on one side only.
                } else {
                    Durability_t oldDurability = pItem->getDurability();
                    repairItem(pItem);
                    if (pItem->getDurability() != oldDurability) {
                        // To cut down DB queries,
                        // save only when the durability changed.
                        snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
                        pItem->tinysave(pField);
                    }

                    if (oldDurability == 0)
                        bSendRealWearingInfo = true;
                }
            }
        }

        // Take the money, and...
        // by sigi.2002.9.4
        pVampire->decreaseGoldEx(repairPrice);


        // Send the OK packet.
        response.setCode(NPC_RESPONSE_REPAIR_OK);
        response.setParameter(pVampire->getGold());
        pPlayer->sendPacket(&response);
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);

        // Compute the repair price summed over every item.
        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            Item* pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL) {
                if (i == Ousters::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // A two-handed weapon is repaired on one side only.
                } else {
                    repairPrice += de::gameContext().prices().getRepairPrice(pItem);
                }
            }
        }

        // Return if there is not enough money.
        if (pOusters->getGold() < repairPrice) {
            response.setCode(NPC_RESPONSE_REPAIR_FAIL_MONEY);
            pPlayer->sendPacket(&response);
            return;
        }

        // Repair each item and save it to the DB.
        char pField[80];

        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            Item* pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL) {
                if (i == Ousters::WEAR_RIGHTHAND && isTwohandWeapon(pItem)) {
                    // A two-handed weapon is repaired on one side only.
                } else {
                    Durability_t oldDurability = pItem->getDurability();
                    repairItem(pItem);
                    if (pItem->getDurability() != oldDurability) {
                        // To cut down DB queries,
                        // save only when the durability changed.
                        snprintf(pField, sizeof(pField), "Durability=%d", pItem->getDurability());
                        pItem->tinysave(pField);
                    }

                    if (oldDurability == 0)
                        bSendRealWearingInfo = true;
                }
            }
        }

        // Take the money, and...
        // by sigi.2002.9.4
        pOusters->decreaseGoldEx(repairPrice);


        // Send the OK packet.
        response.setCode(NPC_RESPONSE_REPAIR_OK);
        response.setParameter(pOusters->getGold());
        pPlayer->sendPacket(&response);
    }

    if (bSendRealWearingInfo) {
        if (pPC->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
            Assert(pSlayer != NULL);

            pSlayer->initAllStatAndSend();
            pSlayer->sendRealWearingInfo();
        } else if (pPC->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
            Assert(pVampire != NULL);

            pVampire->initAllStatAndSend();
            pVampire->sendRealWearingInfo();
        } else if (pPC->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
            Assert(pOusters != NULL);

            pOusters->initAllStatAndSend();
            pOusters->sendRealWearingInfo();
        }
    }

#endif

    __END_DEBUG_EX __END_CATCH
}
