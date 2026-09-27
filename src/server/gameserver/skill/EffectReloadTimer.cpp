//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectReloadTimer.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectReloadTimer.h"

#include <stdio.h>

#include "Creature.h"
#include "GCReloadOK.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "Player.h"
#include "Slayer.h"
#include "item/AR.h"
#include "item/Belt.h"
#include "item/Magazine.h"
#include "item/SG.h"
#include "item/SMG.h"
#include "item/SR.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////////////////
EffectReloadTimer::EffectReloadTimer(Creature* pCreature)

{
    __BEGIN_TRY

    setTarget(pCreature);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// EffectReloadTimer::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectReloadTimer::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// EffectReloadTimer::affect()
//////////////////////////////////////////////////////////////////////////////
void EffectReloadTimer::affect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// EffectReloadTimer::unaffect()
//////////////////////////////////////////////////////////////////////////////
void EffectReloadTimer::unaffect()

{
    __BEGIN_TRY

    unaffect((Creature*)m_pTarget);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// EffectReloadTimer::unaffect()
//////////////////////////////////////////////////////////////////////////////
void EffectReloadTimer::unaffect(Creature* pCreature)

{
    __BEGIN_TRY


    Assert(pCreature != NULL);
    Assert(pCreature->isSlayer());

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
    Player* pPlayer = pSlayer->getPlayer();
    Item* pArmsItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
    Bullet_t BulletNum = 0;
    Item* pItem = NULL;
    Item* pBelt = NULL;
    Inventory* pInventory = NULL;
    bool bSuccess = false;

    // Remove the flag whether the reload succeeds or fails.
    pSlayer->removeFlag(Effect::EFFECT_CLASS_RELOAD_TIMER);

    if (pArmsItem != NULL) {
        if (isArmsWeapon(pArmsItem)) {
            if (m_bFromInventory) {
                // When reloading directly from the inventory,
                // find the item inside the inventory.
                pInventory = pSlayer->getInventory();
                pItem = pInventory->getItem(m_invenX, m_invenY);
            } else {
                // When reloading from the belt,
                // find the item inside the belt.
                if (pSlayer->isWear(Slayer::WEAR_BELT)) {
                    pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);
                    pInventory = ((Belt*)pBelt)->getInventory();
                    pItem = pInventory->getItem(m_SlotID, 0);
                }
            }

            if (pItem == NULL || pInventory == NULL) {
                return;
            }

            ObjectID_t ItemObjectID = pItem->getObjectID();

            // Check that the item is there and that its ObjectID matches.
            if (ItemObjectID == m_ObjectID && pItem->getItemClass() == Item::ITEM_CLASS_MAGAZINE) {
                BulletNum = reloadArmsItem(pArmsItem, pItem);

                // Save it if the reload went through.
                if (BulletNum != 0) {
                    // Item save optimization
                    // by sigi. 2002.5.16
                    char pField[80];
                    snprintf(pField, sizeof(pField), "BulletCount=%d, Silver=%d", pArmsItem->getBulletCount(),
                             pArmsItem->getSilver());
                    pArmsItem->tinysave(pField);

                    // If there are two or more magazines...
                    if (pItem->getNum() > 1) {
                        // reduce the item count, and
                        // reduce the inventory's total count and weight.
                        pItem->setNum(pItem->getNum() - 1);
                        pInventory->decreaseItemNum();
                        pInventory->decreaseWeight(pItem->getWeight());

                        // Save the reduced item count.
                        if (m_bFromInventory) {
                            snprintf(pField, sizeof(pField), "Num=%d", pItem->getNum());
                            pItem->tinysave(pField);
                        } else {
                            snprintf(pField, sizeof(pField), "Num=%d", pItem->getNum());
                            pItem->tinysave(pField);
                        }
                    }
                    // If there is only one magazine, it has to be deleted.
                    else {
                        if (m_bFromInventory)
                            pInventory->deleteItem(m_invenX, m_invenY);
                        else
                            pInventory->deleteItem(m_SlotID, 0);

                        pItem->destroy();
                        SAFE_DELETE(pItem);
                    }

                    bSuccess = true;
                } // if (BulletNum != 0)
            }
        }
    }

    if (bSuccess) {
        GCReloadOK ok;
        ok.setBulletNum(BulletNum);
        pPlayer->sendPacket(&ok);
    }


    __END_CATCH
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectReloadTimer::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

    {__BEGIN_TRY __END_CATCH}


//////////////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////////////
string EffectReloadTimer::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "EffectReloadTimer(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();

    __END_CATCH
}
