//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPickupMoneyHandler.cc
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGPickupMoney.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GCDeleteObject.h"
#include "GCDeleteandPickUpOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "item/Money.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGPickupMoneyHandler::execute(CGPickupMoney* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Creature* pCreature = pGamePlayer->getCreature();
        bool bSuccess = false;
        bool bMargin = false;
        Gold_t itemGold = 0;
        Gold_t currentGold = 0;
        Gold_t marginGold = 0;
        Slayer* pSlayer = NULL;
        Vampire* pVampire = NULL;
        Ousters* pOusters = NULL;
        Zone* pZone = NULL;
        Coord_t ZoneX = pPacket->getZoneX();
        Coord_t ZoneY = pPacket->getZoneY();

        if (pCreature->isSlayer()) {
            pSlayer = dynamic_cast<Slayer*>(pCreature);
            pZone = pSlayer->getZone();
        } else if (pCreature->isVampire()) {
            pVampire = dynamic_cast<Vampire*>(pCreature);
            pZone = pVampire->getZone();
        } else if (pCreature->isOusters()) {
            pOusters = dynamic_cast<Ousters*>(pCreature);
            pZone = pOusters->getZone();
        } else
            throw ProtocolException("CGDropMoneyHandler::execute() : unknown player creature.");

        Assert(pZone != NULL);

        // The bounds must not be crossed.
        if (!isValidZoneCoord(pZone, ZoneX, ZoneY)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        // Return when there is no item on the tile.
        Tile& _Tile = pZone->getTile(ZoneX, ZoneY);
        if (!_Tile.hasItem()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        Item* pItem = _Tile.getItem();
        Assert(pItem != NULL);
        Item::ItemClass IClass = pItem->getItemClass();
        ObjectID_t ObjectID = pItem->getObjectID();

        // Check that the item really is money and that the object ID matches.
        if ((IClass == Item::ITEM_CLASS_MONEY) && (ObjectID == pPacket->getObjectID())) {
            Money* pMoney = dynamic_cast<Money*>(pItem);
            itemGold = pMoney->getAmount();
            marginGold = 0;

            if (pCreature->isSlayer())
                currentGold = pSlayer->getGold();
            else if (pCreature->isVampire())
                currentGold = pVampire->getGold();
            else if (pCreature->isOusters())
                currentGold = pOusters->getGold();

            // The other side's money cannot be picked up.

            if ((pCreature->isSlayer() && pItem->getItemType() != 0) ||
                (pCreature->isVampire() && pItem->getItemType() != 1) ||
                (pCreature->isOusters() && pItem->getItemType() != 2)) {
                GCCannotAdd _GCCannotAdd;
                _GCCannotAdd.setObjectID(pPacket->getObjectID());
                pPlayer->sendPacket(&_GCCannotAdd);
                return;
            }


            if (currentGold + itemGold > MAX_MONEY) {
                // When the money on the ground added to the money held would exceed the maximum,
                // only part of the money on the ground is picked up.
                Gold_t pickupMoney = MAX_MONEY - currentGold;
                marginGold = currentGold + itemGold - MAX_MONEY;
                pMoney->setAmount(marginGold);

                if (pCreature->isSlayer())
                    pSlayer->increaseGoldEx(pickupMoney);
                else if (pCreature->isVampire())
                    pVampire->increaseGoldEx(pickupMoney);
                else if (pCreature->isOusters())
                    pOusters->increaseGoldEx(pickupMoney);

                bSuccess = true;
                bMargin = true;
            } else {
                if (pCreature->isSlayer())
                    pSlayer->increaseGoldEx(itemGold);
                else if (pCreature->isVampire())
                    pVampire->increaseGoldEx(itemGold);
                else if (pCreature->isOusters())
                    pOusters->increaseGoldEx(itemGold);

                bSuccess = true;
                bMargin = false;
            }
        } else // When it is not money, just
        {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }

        if (bSuccess) {
            // First delete the item from the zone.
            pZone->deleteItem(pItem, ZoneX, ZoneY);

            GCDeleteandPickUpOK _GCDeleteandPickUpOK;
            _GCDeleteandPickUpOK.setObjectID(pItem->getObjectID());
            pPlayer->sendPacket(&_GCDeleteandPickUpOK);

            if (pCreature->isSlayer()) {
                // Tell the others nearby that the item has disappeared.
                GCDeleteObject _GCDeleteObject;
                _GCDeleteObject.setObjectID(pItem->getObjectID());
                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCDeleteObject, pSlayer);
            } else if (pCreature->isVampire()) {
                // Tell the others nearby that the item has disappeared.
                GCDeleteObject _GCDeleteObject;
                _GCDeleteObject.setObjectID(pItem->getObjectID());
                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &_GCDeleteObject, pVampire);
            } else if (pCreature->isOusters()) {
                // Tell the others nearby that the item has disappeared.
                GCDeleteObject _GCDeleteObject;
                _GCDeleteObject.setObjectID(pItem->getObjectID());
                pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCDeleteObject, pOusters);
            }

            // If money is left over, create money for the leftover amount and drop it again.
            if (bMargin) {
                TPOINT pt = pZone->addItem(pItem, ZoneX, ZoneY);
                if (pt.x != -1) {
                    char pField[80];
                    snprintf(pField, sizeof(pField), "Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                             pZone->getZoneID(), pt.x, pt.y);
                    pItem->tinysave(pField);
                } else {
                    // If the money could not be dropped, just delete it.

                    SAFE_DELETE(pItem);
                }
            } else {
                pItem->destroy();
                SAFE_DELETE(pItem);
            }

            // Leave a money log if the amount warrants one
            if ((itemGold - marginGold) >= de::gameContext().variables().getMoneyTraceLogLimit()) {
                char zoneName[15];
                snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), ZoneX, ZoneY);
                remainMoneyTraceLog(zoneName, pCreature->getName(), ITEM_LOG_MOVE, DETAIL_PICKUP,
                                    itemGold - marginGold);
            }
        } else {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG
    __END_DEBUG_EX __END_CATCH
}
