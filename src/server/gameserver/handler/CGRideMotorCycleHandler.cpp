//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRideMotorCycleHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGRideMotorCycle.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "CreatureUtil.h"
#include "Effect.h"
#include "GCRideMotorCycle.h"
#include "GCRideMotorCycleFailed.h"
#include "GCRideMotorCycleOK.h"
#include "GQuestManager.h"
#include "GamePlayer.h"
#include "RelicUtil.h"
#include "SiegeManager.h"
#include "SkillHandlerManager.h"
#include "ZoneUtil.h"
#include "item/Belt.h"
#include "item/Motorcycle.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGRideMotorCycleHandler::execute(CGRideMotorCycle* pPacket, Player* pPlayer) {
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
            Creature* pCreature = pGamePlayer->getCreature();
            bool Success = false;

            Assert(pCreature != NULL);

            if (pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET))
                return;
            if (pCreature->isFlag(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1)) {
                Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_BURNING_SOL_CHARGE_1);
                if (pEffect != NULL)
                    pEffect->setDeadline(0);
            }

            if (pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                Zone* pZone = pSlayer->getZone();
                Motorcycle* pMotor = pSlayer->getMotorcycle();
                ZoneCoord_t X = pPacket->getX();
                ZoneCoord_t Y = pPacket->getY();

                // The coordinates must not cross the bounds.
                if (!isValidZoneCoord(pZone, X, Y) || SiegeManager::Instance().isSiegeZone(pZone->getZoneID())) {
                    GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                    pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
                    return;
                }

                Tile& rTile = pZone->getTile(X, Y);
                Item* pItem = rTile.getItem();

                if (pSlayer->hasRelicItem() || pSlayer->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
                    pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                    pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
                    GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                    pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
                    return;
                }

                if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_MOTORCYCLE) {
                    GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                    pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
                    return;
                }

                if (pItem->getItemType() > 5 && !pSlayer->isAdvanced()) {
                    GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                    pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
                    return;
                }

                if (pSlayer->isDead() || pSlayer->isFlag(Effect::EFFECT_CLASS_COMA)) {
                    GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                    pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
                    return;
                }

                if (pItem->getItemClass() == Item::ITEM_CLASS_MOTORCYCLE && pMotor == NULL) {
                    Motorcycle* pMotorcycle = dynamic_cast<Motorcycle*>(pItem);
                    ItemID_t TargetItemID = pMotorcycle->getItemID();
                    Inventory* pInventory = pSlayer->getInventory();

                    Item* pBelt = NULL;

                    pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);
                    Inventory* pBeltInventory = NULL;
                    if (pBelt != NULL) {
                        pBeltInventory = ((Belt*)pBelt)->getInventory();
                    }

                    if (pInventory->hasKey(TargetItemID) ||
                        (pBeltInventory != NULL && pBeltInventory->hasKey(TargetItemID))) {
                        pSlayer->setMotorcycle(pMotorcycle);

                        GCRideMotorCycle _GCRideMotorCycle;
                        GCRideMotorCycleOK _GCRideMotorCycleOK;

                        _GCRideMotorCycle.setObjectID(pSlayer->getObjectID());
                        _GCRideMotorCycle.setTargetObjectID(pMotorcycle->getObjectID());
                        _GCRideMotorCycleOK.setObjectID(pMotorcycle->getObjectID());

                        // Erase the item from the zone, and...
                        pZone->deleteItem(pItem, X, Y);
                        // send the OK packet to the player, and...
                        pGamePlayer->sendPacket(&_GCRideMotorCycleOK);
                        // broadcast that the motorcycle is being ridden
                        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCRideMotorCycle);
                        Success = true;

                        if (pSlayer->getPetInfo() != NULL) {
                            pSlayer->setPetInfo(NULL);
                            sendPetInfo(pGamePlayer, true);
                        }

                        pSlayer->getGQuestManager()->rideMotorcycle();
                    } else {
                    }
                }
            }

            if (!Success) {
                GCRideMotorCycleFailed _GCRideMotorCycleFailed;
                pGamePlayer->sendPacket(&_GCRideMotorCycleFailed);
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
