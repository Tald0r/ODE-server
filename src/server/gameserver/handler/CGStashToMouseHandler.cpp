//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStashToMouseHandler.cpp
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGStashToMouse.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "GCCannotAdd.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "PlayerCreature.h"
#include "Stash.h"
#include "Zone.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGStashToMouseHandler::execute(CGStashToMouse* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Creature* pCreature = pGamePlayer->getCreature();
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    BYTE rack = pPacket->getRack();
    BYTE index = pPacket->getIndex();

    // Going past the maximum range, and
    // going past one's own maximum rack, are both errors.
    if (rack >= STASH_RACK_MAX || index >= STASH_INDEX_MAX || rack >= pPC->getStashNum()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    Stash* pStash = pPC->getStash();
    Item* pStashItem = pStash->get(rack, index);
    Item* pMouseItem = pPC->getExtraInventorySlotItem();

    // When the stash holds no item, or an item hangs on the mouse,
    // another item cannot be hung on the mouse.
    if (pStashItem == NULL || pMouseItem != NULL) {
        // throw ProtocolException("CGStashToMouseHandler::executeSlayer() : there is no item, or an item hangs on
        // the mouse.");
        //  With no item, or with an item already on the mouse, nothing can be added.
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    ObjectID_t ObjectID = pStashItem->getObjectID();

    if (ObjectID != pPacket->getObjectID()) {
        GCCannotAdd _GCCannotAdd;
        _GCCannotAdd.setObjectID(pPacket->getObjectID());
        pPlayer->sendPacket(&_GCCannotAdd);
        return;
    }

    // Remove the item from the stash, hang it on the mouse, then save to the DB.
    pStash->remove(rack, index);
    pPC->addItemToExtraInventorySlot(pStashItem);
    char pField[80];
    snprintf(pField, sizeof(pField), "Storage=%d", STORAGE_EXTRASLOT);
    pStashItem->tinysave(pField);


#endif // __GAME_SERVER__

    __END_DEBUG
    __END_DEBUG_EX __END_CATCH
}
