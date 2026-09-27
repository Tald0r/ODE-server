//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddMouseToZoneHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAddMouseToZone.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "Belt.h"
#include "CreatureUtil.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectRelicLock.h"
#include "EffectRelicPosition.h"
#include "GCAddNewItemToZone.h"
#include "GCCannotAdd.h"
#include "GCDeleteObject.h"
#include "GCRemoveEffect.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Ousters.h"
#include "OustersArmsband.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "Slayer.h"
#include "StringPool.h"
#include "StringStream.h"
#include "Sweeper.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAddMouseToZoneHandler::execute(CGAddMouseToZone* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    bool Success = false;


    Zone* pZone = pCreature->getZone();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    InventorySlot* pExtraInventorySlot = pPC->getExtraInventorySlot();
    Item* pItem = pExtraInventorySlot->getItem();

    if (pItem == NULL) {
        // throw ProtocolException("CGAddMouseToZoneHandler::execute() : the item to drop in the zone does not
        // exist.");
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        if (pSlayer->hasRideMotorcycle()) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }
    }

    if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
            GCCannotAdd _GCCannotAdd;
            _GCCannotAdd.setObjectID(pPacket->getObjectID());
            pPlayer->sendPacket(&_GCCannotAdd);
            return;
        }
    }

    ObjectID_t ItemObjectID = pItem->getObjectID();

    if (ItemObjectID == pPacket->getObjectID() && canDropToZone(pPC, pItem)) {
        // Access the Item in the Zone.
        // For now drop the Item at one's own position.
        // Later the Tile the item fell on has to be found.
        // A CREATE_TYPE_GAME item disappears after 10 seconds.
        Turn_t decayTurn = 0;
        if (pItem->getCreateType() == Item::CREATE_TYPE_GAME)
            decayTurn = 100;

        TPOINT pt = pZone->addItem(pItem, pCreature->getX(), pCreature->getY(), true, decayTurn);

        if (pt.x != -1) {
            pItem->whenPCLost(pPC);
            char pField[80];
            snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", (int)STORAGE_ZONE,
                     pZone->getZoneID(), pt.x, pt.y);
            pItem->tinysave(pField);

            // For a belt, the items inside it must lose their owner too.
            if (pItem->getItemClass() == Item::ITEM_CLASS_BELT) {
                snprintf(pField, sizeof(pField), "OwnerID=''");

                Belt* pBelt = dynamic_cast<Belt*>(pItem);
                Assert(pBelt != NULL);

                Inventory* pBeltInventory = pBelt->getInventory();
                PocketNum_t num = pBelt->getPocketCount();

                for (SlotID_t count = 0; count < num; ++count) {
                    Item* pBeltItem = pBeltInventory->getItem(count, 0);
                    if (pBeltItem != NULL) {
                        pBeltItem->tinysave(pField);
                    }
                }
            }

            // For an armsband, the items inside it must lose their owner too.
            if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_ARMSBAND) {
                snprintf(pField, sizeof(pField), "OwnerID=''");

                OustersArmsband* pOustersArmsband = dynamic_cast<OustersArmsband*>(pItem);
                Assert(pOustersArmsband != NULL);

                Inventory* pOustersArmsbandInventory = pOustersArmsband->getInventory();
                PocketNum_t num = pOustersArmsband->getPocketCount();

                for (SlotID_t count = 0; count < num; ++count) {
                    Item* pOustersArmsbandItem = pOustersArmsbandInventory->getItem(count, 0);
                    if (pOustersArmsbandItem != NULL) {
                        pOustersArmsbandItem->tinysave(pField);
                    }
                }
            }

            pPC->deleteItemFromExtraInventorySlot();

            if (pItem->isFlagItem()) {
                Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_FLAG);

                if (pEffect != NULL)
                    pEffect->setDeadline(0);

                if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
                    EffectRelicLock* pLock = new EffectRelicLock(pItem);
                    pLock->setDeadline(10 * 10); // 10 seconds
                    pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
                    pItem->getEffectManager().addEffect(pLock);
                }
            }

            if (pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
                Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_SWEEPER);
                if (pEffect != NULL)
                    pEffect->setDeadline(0);

                if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
                    EffectRelicLock* pLock = new EffectRelicLock(pItem);
                    pLock->setDeadline(10 * 10); // 10 seconds
                    pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
                    pItem->getEffectManager().addEffect(pLock);
                }

                // Broadcast the drop to the zone.
                char race[15];
                if (pCreature->isSlayer()) {
                    snprintf(race, sizeof(race), strings.c_str(STRID_SLAYER));
                } else if (pCreature->isVampire()) {
                    snprintf(race, sizeof(race), strings.c_str(STRID_VAMPIRE));
                } else if (pCreature->isOusters()) {
                    snprintf(race, sizeof(race), strings.c_str(STRID_OUSTERS));
                } else {
                    Assert(false);
                }

                const SweeperInfo* pSweeperInfo = dynamic_cast<SweeperInfo*>(
                    de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_SWEEPER, pItem->getItemType()));

                char msg[100];
                snprintf(msg, sizeof(msg), strings.c_str(STRID_DROP_SWEEPER), pSweeperInfo->getName().c_str(),
                         pCreature->getName().c_str(), race);
                GCSystemMessage gcSystemMessage;
                gcSystemMessage.setMessage(msg);
                pZone->broadcastPacket(&gcSystemMessage);
            }


            // When a Relic is put on the ground, remove the Effect.
            if (isRelicItem(pItem)) {
                deleteRelicEffect(pPC, pItem);

                // Attach an effect so that it cannot be picked up for a while.
                if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
                    EffectRelicLock* pLock = new EffectRelicLock(pItem);
                    pLock->setDeadline(10 * 10); // 10 seconds
                    pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
                    pItem->getEffectManager().addEffect(pLock);
                } else {
                    // Find the existing one and extend its time.
                }

                // Report where the Relic fell from time to time.

                // Send every user a message that a Relic has fallen.
            }


            // Leave an ItemTrace
            if (pItem != NULL && pItem->isTraceItem()) {
                char zoneName[15];
                snprintf(zoneName, sizeof(zoneName), "%4d%3d%3d", pZone->getZoneID(), pt.x, pt.y);
                remainTraceLog(pItem, pCreature->getName(), zoneName, ITEM_LOG_MOVE, DETAIL_DROP);
            }

            Success = true;

            if (pItem->isQuestItem() ||
                (pItem->getItemClass() == Item::ITEM_CLASS_MOON_CARD && pItem->getItemType() == 2)) {
                // A quest item is deleted right away.
                // Because of the stupid client, the one on the mouse cannot be deleted right away.
                // So it is handled this way.
                pZone->deleteItem(pItem, pt.x, pt.y);

                GCDeleteObject gcDeleteObject;
                gcDeleteObject.setObjectID(pItem->getObjectID());

                pZone->broadcastPacket(pt.x, pt.y, &gcDeleteObject);
            }
        } else {
            Success = false;
        }
    }

    // When adding failed.
    if (!Success) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
