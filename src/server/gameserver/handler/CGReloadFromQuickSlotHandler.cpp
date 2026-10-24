//////////////////////////////////////////////////////////////////////////////
// Filename    : CGReloadFromQuickSlotHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGReloadFromQuickSlot.h"

#ifdef __GAME_SERVER__
#include "EffectManager.h"
#include "GCCannotUse.h"
#include "GamePlayer.h"
#include "Inventory.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Slayer.h"
#include "Zone.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/Magazine.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"
#include "skill/EffectReloadTimer.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGReloadFromQuickSlotHandler::execute(CGReloadFromQuickSlot* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Creature* pCreature = pGamePlayer->getCreature();

        Assert(pCreature->isSlayer());

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Item* pArmsItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        Item* pBeltItem = NULL;
        bool Success = false;
        ObjectID_t ItemObjectID = 0;
        SlotID_t SlotID;

        if (pArmsItem != NULL) {
            if (isArmsWeapon(pArmsItem)) {
                Item* pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);

                if (pBelt == NULL) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pPlayer->sendPacket(&_GCCannotUse);
                    return;
                }

                Inventory* pBeltInventory = ((Belt*)pBelt)->getInventory();
                SlotID = pPacket->getSlotID();

                if (SlotID >= pBeltInventory->getWidth()) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pPlayer->sendPacket(&_GCCannotUse);
                    return;
                }

                pBeltItem = pBeltInventory->getItem(SlotID, 0);

                if (pBeltItem == NULL) {
                    GCCannotUse _GCCannotUse;
                    _GCCannotUse.setObjectID(pPacket->getObjectID());
                    pPlayer->sendPacket(&_GCCannotUse);
                    return;
                }

                // Get the Object of the item in the slot.
                ItemObjectID = pBeltItem->getObjectID();

                // Check that the item exists and that its ObjectID matches.
                if (ItemObjectID == pPacket->getObjectID() && pBeltItem->getItemClass() == Item::ITEM_CLASS_MAGAZINE) {
                    SkillSlot* pVivid = pSlayer->getSkill(SKILL_VIVID_MAGAZINE);
                    bool hasVivid = (pVivid != NULL) && pVivid->canUse();

                    if (isSuitableMagazine(pArmsItem, pBeltItem, hasVivid)) {
                        Success = true;
                    }
                }
            }
        }

        // Send the verify packet.
        EffectManager* pEffectManager = pSlayer->getEffectManager();
        if (Success && !pSlayer->isFlag(Effect::EFFECT_CLASS_RELOAD_TIMER)) {
            EffectReloadTimer* pEffect = new EffectReloadTimer(pSlayer);
            pEffect->setFromInventory(false);
            pEffect->setObjectID(ItemObjectID);
            pEffect->setSlotID(SlotID);
            if (pSlayer->hasSkill(SKILL_FAST_RELOAD)) {
                pEffect->setDeadline(7); // fast reload (0.7 sec)
            } else {
                pEffect->setDeadline(2 * 10); // ordinary reload (2sec)
            }

            pSlayer->setFlag(Effect::EFFECT_CLASS_RELOAD_TIMER);
            pEffectManager->addEffect(pEffect);
        } else {
            GCCannotUse _GCCannotUse;
            _GCCannotUse.setObjectID(ItemObjectID);

            pPlayer->sendPacket(&_GCCannotUse);
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
