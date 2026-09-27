//////////////////////////////////////////////////////////////////////////////
// Filename    : CGDropMoneyHandler.cc
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGDropMoney.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "item/Money.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGDropMoneyHandler::execute(CGDropMoney* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        throw DisconnectException("Money cannot be dropped.");

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Creature* pPC = pGamePlayer->getCreature();
        Gold_t amount = pPacket->getAmount();
        Slayer* pSlayer = NULL;
        Vampire* pVampire = NULL;
        Ousters* pOusters = NULL;
        Zone* pZone = NULL;
        ZoneCoord_t ZoneX = pPC->getX();
        ZoneCoord_t ZoneY = pPC->getY();

        if (amount == 0) {
            // For now just return.
            // Should a packet saying the money drop failed be made?
            return;
        }

        if (pPC->isSlayer()) {
            pSlayer = dynamic_cast<Slayer*>(pPC);
            pZone = pSlayer->getZone();
        } else if (pPC->isVampire()) {
            pVampire = dynamic_cast<Vampire*>(pPC);
            pZone = pVampire->getZone();
        } else if (pPC->isOusters()) {
            pOusters = dynamic_cast<Ousters*>(pPC);
            pZone = pOusters->getZone();
        } else
            throw ProtocolException("CGDropMoneyHandler::execute() : unknown player creature.");

        // Check that the player holds as much money as it wants to drop.
        Item* pItem = NULL;
        list<OptionType_t> optionNULL;
        ItemFactoryManager& itemFactories = de::gameContext().itemFactories();
        if (pPC->isSlayer()) {
            if (pSlayer->getGold() < amount)
                return;

            pItem = itemFactories.createItem(Item::ITEM_CLASS_MONEY, 0, optionNULL);
        } else if (pPC->isVampire()) {
            if (pVampire->getGold() < amount)
                return;

            pItem = itemFactories.createItem(Item::ITEM_CLASS_MONEY, 1, optionNULL);
        } else if (pPC->isOusters()) {
            if (pOusters->getGold() < amount)
                return;

            pItem = itemFactories.createItem(Item::ITEM_CLASS_MONEY, 2, optionNULL);
        }

        // Create the money item.

        dynamic_cast<Money*>(pItem)->setAmount(amount);

        // Register the money item.
        pZone->getObjectRegistry().registerObject(pItem);

        // Drop the money in the zone.
        TPOINT pt = pZone->addItem(pItem, ZoneX, ZoneY);
        if (pt.x != -1) {
            char pField[80];
            snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                     pZone->getZoneID(), pt.x, pt.y);
            pItem->tinysave(pField);


            // Leave a money log if the amount warrants one
            if (amount >= de::gameContext().variables().getMoneyTraceLogLimit()) {
                char zoneName[15];
                snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), ZoneX, ZoneY);
                remainMoneyTraceLog(pPC->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP, amount);
            }
        } else {
            SAFE_DELETE(pItem);
            return;
        }


        // by sigi. 2002.9.4
        if (pPC->isSlayer())
            pSlayer->decreaseGoldEx(amount);
        else if (pPC->isVampire())
            pVampire->decreaseGoldEx(amount);
        else if (pPC->isOusters())
            pOusters->decreaseGoldEx(amount);
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG
    __END_DEBUG_EX __END_CATCH
}
