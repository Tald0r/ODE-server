//////////////////////////////////////////////////////////////////////////////
// Filename    : CGGetEventItemHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGGetEventItem.h"

#ifdef __GAME_SERVER__
#include "Assert.h"
#include "GCCreateItem.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "NicknameBook.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Zone.h"
#include "repository/ComebackEventRepository.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGGetEventItemHandler::execute(CGGetEventItem* pPacket, Player* pPlayer) {
#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    if (pPacket->getEventType() == EVENT_TYPE_200501_COMBACK_ITEM) {
        executeCombackItem(pPacket, pPlayer);
    } else if (pPacket->getEventType() == EVENT_TYPE_200501_COMBACK_PREMIUM_ITEM) {
        executeCombackPremiumItem(pPacket, pPlayer);
    } else if (pPacket->getEventType() == EVENT_TYPE_200501_COMBACK_RECOMMEND_ITEM) {
        executeCombackRecommendItem(pPacket, pPlayer);
    }

#endif
}


#ifdef __GAME_SERVER__

void CGGetEventItemHandler::executeCombackItem(CGGetEventItem* pPacket, Player* pPlayer) {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    static int i = 0;

    {
        string RecvItemDate;

        if (defaultComebackEventRepository().loadMainRecvItemDate(pGamePlayer->getID(), RecvItemDate)) {
            if (RecvItemDate == "0000-00-00") {
                // Give the item
                Creature* pCreature = pGamePlayer->getCreature();
                Assert(pCreature != NULL);

                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                Zone* pZone = pPC->getZone();
                Assert(pZone != NULL);

                Inventory* pInventory = pPC->getInventory();
                Assert(pInventory != NULL);

                Level_t level = pPC->getLevel();
                Race_t race = pPC->getRace();

                Item::ItemClass iClass = Item::ITEM_CLASS_MAX;
                ItemType_t iType = 0;
                Grade_t iGrade = 6;
                list<OptionType_t> optionType;

                // Decide the option
                i = i ^ 1;
                if (i) {
                    optionType.push_back(50);  // DAM+3
                    optionType.push_back(176); // LUCK+3
                } else {
                    optionType.push_back(18);  // HP+9
                    optionType.push_back(184); // ATTR+2
                }

                // Race
                if (race == RACE_SLAYER) {
                    iClass = Item::ITEM_CLASS_RING;
                } else if (race == RACE_VAMPIRE) {
                    iClass = Item::ITEM_CLASS_VAMPIRE_RING;
                } else if (race == RACE_OUSTERS) {
                    iClass = Item::ITEM_CLASS_OUSTERS_RING;
                } else {
                    Assert(false);
                }

                // Level
                if (level <= 30) {
                    iType = 3;
                } else if (level <= 60) {
                    iType = 4;
                } else if (level <= 90) {
                    iType = 4;
                } else if (level <= 120) {
                    iType = 5;
                } else {
                    iType = 5;
                }

                if (iClass == Item::ITEM_CLASS_MAX) {
                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(FAIL_GET_EVENT_ITEM);
                    pPlayer->sendPacket(&response);
                    return;
                }

                Item* pItem = de::gameContext().itemFactories().createItem(iClass, iType, optionType);
                if (pItem == NULL) {
                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(FAIL_GET_EVENT_ITEM);
                    pPlayer->sendPacket(&response);
                    return;
                }

                pItem->setGrade(iGrade);

                // Check for a free space
                _TPOINT tp;
                if (!pInventory->getEmptySlot(pItem, tp)) {
                    SAFE_DELETE(pItem);

                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                    pPlayer->sendPacket(&response);
                    return;
                }

                filelog("CombackLog.txt", "[%s:%s] get %s", pGamePlayer->getID().c_str(), pPC->getName().c_str(),
                        pItem->toString().c_str());

                pItem->setTraceItem(bTraceLog(pItem));
                pZone->registerObject(pItem);

                if (pItem->isTraceItem()) {
                    remainTraceLog(pItem, "", pPC->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
                }

                if (!pInventory->addItem(pItem, tp)) {
                    SAFE_DELETE(pItem);

                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                    pPlayer->sendPacket(&response);
                    return;
                }

                pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, tp.x, tp.y);

                GCCreateItem gcCI;
                makeGCCreateItem(&gcCI, pItem, tp.x, tp.y);

                pPlayer->sendPacket(&gcCI);


                // Record in the DB that it was taken.
                defaultComebackEventRepository().markMainItemReceived(pGamePlayer->getID());

                // Send the client a message that it was received.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);

                // Add the nickname and send the packet to the client.
                NicknameBook* pNicknameBook = pPC->getNicknameBook();
                Assert(pNicknameBook != NULL);

                pNicknameBook->addNewNickname("Come Back!");
                pGamePlayer->sendPacket(pNicknameBook->getNicknameBookListPacket().get());
            } else {
                // The item was already taken.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(ALEADY_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);
                return;
            }
        } else {
            // Not one of the targets.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
            response.setParameter(YOU_ARE_NOT_EVENT_TARGET);
            pPlayer->sendPacket(&response);
            return;
        }
    }
}


void CGGetEventItemHandler::executeCombackPremiumItem(CGGetEventItem* pPacket, Player* pPlayer) {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    {
        string PayPremiumDate;
        string RecvItemDate;

        if (defaultComebackEventRepository().loadMainPremiumDates(pGamePlayer->getID(), PayPremiumDate, RecvItemDate)) {
            if (PayPremiumDate == "0000-00-00") {
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_ARE_NOT_EVENT_TARGET);
                pPlayer->sendPacket(&response);
                return;
            }

            if (RecvItemDate == "0000-00-00") {
                // Give the item
                Creature* pCreature = pGamePlayer->getCreature();
                Assert(pCreature != NULL);

                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                Zone* pZone = pPC->getZone();
                Assert(pZone != NULL);

                Inventory* pInventory = pPC->getInventory();
                Assert(pInventory != NULL);

                Race_t race = pPC->getRace();

                Item* pItem[5] = {NULL, NULL, NULL, NULL, NULL};

                ItemFactoryManager& itemFactories = de::gameContext().itemFactories();

                // Option DAM_3
                list<OptionType_t> optionType;
                optionType.push_back(50); // DAM+3

                if (race == RACE_SLAYER) {
                    pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_NECKLACE, 8, optionType); // Blood Cross
                    pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_BRACELET, 8,
                                                        optionType); // Cross Bracelet
                    pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_BRACELET, 8,
                                                        optionType);                           // Cross Bracelet
                    pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_RING, 8, optionType); // Platinum Spiral
                    pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_RING, 8, optionType); // Platinum Spiral
                } else if (race == RACE_VAMPIRE) {
                    pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_NECKLACE, 8,
                                                        optionType); // Black Ankh
                    pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_BRACELET, 7,
                                                        optionType); // Viper Bracelet
                    pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_RING, 8,
                                                        optionType); // Ring of Skulls
                    pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_EARRING, 8,
                                                        optionType); // The Cat's Eye
                    pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_VAMPIRE_AMULET, 8, optionType); // Odal
                } else if (race == RACE_OUSTERS) {
                    pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_OUSTERS_RING, 8,
                                                        optionType); // Mekzan's Ring
                    pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_OUSTERS_RING, 8,
                                                        optionType); // Mekzan's Ring
                    pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_OUSTERS_PENDENT, 8,
                                                        optionType); // Mekzan's Ring
                    pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_OUSTERS_PENDENT, 8,
                                                        optionType); // Fiery Pendant
                    pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_OUSTERS_PENDENT, 8,
                                                        optionType); // Fiery Pendant
                } else {
                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(FAIL_GET_EVENT_ITEM);
                    pPlayer->sendPacket(&response);
                    return;
                }

                for (int i = 0; i < 5; ++i) {
                    if (pItem[i] == NULL) {
                        for (int j = 0; j < 5; ++j) {
                            SAFE_DELETE(pItem[j]);
                        }

                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                        response.setParameter(FAIL_GET_EVENT_ITEM);
                        pPlayer->sendPacket(&response);
                        return;
                    }

                    pItem[i]->setGrade(6);
                }

                _TPOINT tp[5];
                for (int i = 0; i < 5; ++i) {
                    // Check for a free space

                    if (!pInventory->getEmptySlot(pItem[i], tp[i])) {
                        // Clear. Roll back
                        for (int j = 0; j < i; ++j) {
                            pInventory->deleteItem(pItem[j]->getObjectID());
                        }

                        for (int j = 0; j < 5; ++j) {
                            SAFE_DELETE(pItem[j]);
                        }

                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                        response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                        pPlayer->sendPacket(&response);
                        return;
                    }

                    pItem[i]->setTraceItem(bTraceLog(pItem[i]));
                    pZone->registerObject(pItem[i]);

                    if (pItem[i]->isTraceItem()) {
                        remainTraceLog(pItem[i], "", pPC->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
                    }

                    if (!pInventory->addItem(pItem[i], tp[i])) {
                        // Clear. Roll back
                        for (int j = 0; j < i; ++j) {
                            pInventory->deleteItem(pItem[j]->getObjectID());
                        }

                        for (int j = 0; j < 5; ++j) {
                            SAFE_DELETE(pItem[j]);
                        }

                        GCNPCResponse response;
                        response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                        response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                        pPlayer->sendPacket(&response);
                        return;
                    }
                }

                // Create in the DB, leave a log and send to the client
                // Add the time limit
                for (int i = 0; i < 5; ++i) {
                    pItem[i]->create(pPC->getName(), STORAGE_INVENTORY, 0, tp[i].x, tp[i].y);

                    filelog("CombackLog.txt", "[%s:%s] get %s", pGamePlayer->getID().c_str(), pPC->getName().c_str(),
                            pItem[i]->toString().c_str());

                    GCCreateItem gcCI;
                    makeGCCreateItem(&gcCI, pItem[i], tp[i].x, tp[i].y);

                    pPlayer->sendPacket(&gcCI);

                    // Time limit
                    pPC->addTimeLimitItem(pItem[i], 7 * 24 * 60 * 60);
                }

                pPC->sendTimeLimitItemInfo();

                // Record in the DB that it was taken.
                defaultComebackEventRepository().markMainPremiumItemReceived(pGamePlayer->getID());

                // Send the client a message that it was received.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);
            } else {
                // The item was already taken.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(ALEADY_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);
                return;
            }
        } else {
            // Not one of the targets.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
            response.setParameter(YOU_ARE_NOT_EVENT_TARGET);
            pPlayer->sendPacket(&response);
            return;
        }
    }
}


void CGGetEventItemHandler::executeCombackRecommendItem(CGGetEventItem* pPacket, Player* pPlayer) {
    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    static int i = 0;

    {
        int UniqueID = 0;
        string RecvItemDate;

        if (defaultComebackEventRepository().loadRecommendRow(pGamePlayer->getID(), UniqueID, RecvItemDate)) {
            if (RecvItemDate == "0000-00-00") {
                // Give the item
                Creature* pCreature = pGamePlayer->getCreature();
                Assert(pCreature != NULL);

                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                Zone* pZone = pPC->getZone();
                Assert(pZone != NULL);

                Inventory* pInventory = pPC->getInventory();
                Assert(pInventory != NULL);

                Level_t level = pPC->getLevel();
                Race_t race = pPC->getRace();

                Item::ItemClass iClass = Item::ITEM_CLASS_MAX;
                ItemType_t iType = 0;
                Grade_t iGrade = 6;
                list<OptionType_t> optionType;

                // Decide the option
                i = i ^ 1;
                if (i) {
                    optionType.push_back(50);  // DAM+3
                    optionType.push_back(176); // LUCK+3
                } else {
                    optionType.push_back(18);  // HP+9
                    optionType.push_back(184); // ATTR+2
                }

                // Race
                if (race == RACE_SLAYER) {
                    iClass = Item::ITEM_CLASS_RING;
                } else if (race == RACE_VAMPIRE) {
                    iClass = Item::ITEM_CLASS_VAMPIRE_RING;
                } else if (race == RACE_OUSTERS) {
                    iClass = Item::ITEM_CLASS_OUSTERS_RING;
                } else {
                    Assert(false);
                }

                // Level
                if (level <= 30) {
                    iType = 3;
                } else if (level <= 60) {
                    iType = 4;
                } else if (level <= 90) {
                    iType = 4;
                } else if (level <= 120) {
                    iType = 5;
                } else {
                    iType = 5;
                }

                if (iClass == Item::ITEM_CLASS_MAX) {
                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(FAIL_GET_EVENT_ITEM);
                    pPlayer->sendPacket(&response);
                    return;
                }

                Item* pItem = de::gameContext().itemFactories().createItem(iClass, iType, optionType);
                if (pItem == NULL) {
                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(FAIL_GET_EVENT_ITEM);
                    pPlayer->sendPacket(&response);
                    return;
                }

                pItem->setGrade(iGrade);

                // Check for a free space
                _TPOINT tp;
                if (!pInventory->getEmptySlot(pItem, tp)) {
                    SAFE_DELETE(pItem);

                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                    pPlayer->sendPacket(&response);
                    return;
                }

                filelog("CombackLog.txt", "[%s:%s] get %s", pGamePlayer->getID().c_str(), pPC->getName().c_str(),
                        pItem->toString().c_str());

                pItem->setTraceItem(bTraceLog(pItem));
                pZone->registerObject(pItem);

                if (pItem->isTraceItem()) {
                    remainTraceLog(pItem, "", pPC->getName(), ITEM_LOG_CREATE, DETAIL_EVENTNPC);
                }

                if (!pInventory->addItem(pItem, tp)) {
                    SAFE_DELETE(pItem);

                    GCNPCResponse response;
                    response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                    response.setParameter(NOT_ENOUGH_INVENTORY_SPACE);
                    pPlayer->sendPacket(&response);
                    return;
                }

                pItem->create(pPC->getName(), STORAGE_INVENTORY, 0, tp.x, tp.y);

                GCCreateItem gcCI;
                makeGCCreateItem(&gcCI, pItem, tp.x, tp.y);

                pPlayer->sendPacket(&gcCI);

                // Record in the DB that it was taken.
                defaultComebackEventRepository().markRecommendItemReceived(UniqueID);

                // Send the client a message that it was received.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);
            } else {
                // The item was already taken.
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(ALEADY_GET_EVENT_ITEM);
                pPlayer->sendPacket(&response);
                return;
            }
        } else {
            // Not one of the targets.
            GCNPCResponse response;
            response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
            response.setParameter(YOU_ARE_NOT_EVENT_TARGET);
            pPlayer->sendPacket(&response);
            return;
        }
    }
}

#endif
