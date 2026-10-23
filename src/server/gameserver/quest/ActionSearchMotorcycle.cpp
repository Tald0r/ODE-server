////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionSearchMotorcycle.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionSearchMotorcycle.h"

#include "Belt.h"
#include "Creature.h"
#include "DB.h"
#include "GCNPCResponse.h"
#include "GCSearchMotorcycleFail.h"
#include "GCSearchMotorcycleOK.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "NPC.h"
#include "ParkingCenter.h"
#include "Slayer.h"
#include "item/Key.h"
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionSearchMotorcycle::read(PropertyBuffer& propertyBuffer)

{}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionSearchMotorcycle::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    // Send an OK packet to the client first.
    GCNPCResponse answerOKpkt;
    pPlayer->sendPacket(&answerOKpkt);

    // Check whether the player is a Slayer.
    if (pCreature2->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
        Inventory* pInventory = pSlayer->getInventory();
        uint InvenWidth = pInventory->getWidth();
        uint InvenHeight = pInventory->getHeight();
        Item* pItem = NULL;
        uint motorZoneID = 0;
        uint motorX = 0;
        uint motorY = 0;

        Inventory* pBeltInventory = NULL;
        uint BeltInvenWidth = 0;
        uint BeltInvenHeight = 0;
        Item* pBelt = NULL;

        pBelt = pSlayer->getWearItem(Slayer::WEAR_BELT);
        if (pBelt != NULL) {
            pBeltInventory = ((Belt*)pBelt)->getInventory();

            BeltInvenWidth = pBeltInventory->getWidth();
            BeltInvenHeight = pBeltInventory->getHeight();
        }

        // Search the inventory.
        for (uint y = 0; y < InvenHeight; y++) {
            for (uint x = 0; x < InvenWidth; x++) {
                // If there is an item at x, y...
                if (pInventory->hasItem(x, y)) {
                    pItem = pInventory->getItem(x, y);
                    if (search(pItem, motorZoneID, motorX, motorY)) {
                        GCSearchMotorcycleOK okpkt;
                        okpkt.setZoneID(motorZoneID);
                        okpkt.setX(motorX);
                        okpkt.setY(motorY);
                        pPlayer->sendPacket(&okpkt);
                        return;
                    }
                }
            }
        }

        if (pBelt != NULL) {
            // Search the inventory.
            for (uint y = 0; y < BeltInvenHeight; y++) {
                for (uint x = 0; x < BeltInvenWidth; x++) {
                    // If there is an item at x, y...
                    if (pBeltInventory->hasItem(x, y)) {
                        pItem = pBeltInventory->getItem(x, y);
                        if (search(pItem, motorZoneID, motorX, motorY)) {
                            GCSearchMotorcycleOK okpkt;
                            okpkt.setZoneID(motorZoneID);
                            okpkt.setX(motorX);
                            okpkt.setY(motorY);
                            pPlayer->sendPacket(&okpkt);
                            return;
                        }
                    }
                }
            }
        }
    } else // A Vampire has no motorcycle to redeem.
    {
    }

    GCSearchMotorcycleFail failpkt;
    pPlayer->sendPacket(&failpkt);
}

bool ActionSearchMotorcycle::search(Item* pItem, uint& zoneid, uint& x, uint& y) const

{
    bool bFound = false;

    // Return false if the item is not a key.
    if (pItem->getItemClass() != Item::ITEM_CLASS_KEY)
        return false;

    // If it is a key, get the item ID the key targets.
    DWORD targetID = dynamic_cast<Key*>(pItem)->getTarget();

    // Check whether the matching motorcycle exists.
    if (context().parking().hasMotorcycleBox(targetID)) {
        MotorcycleBox* pBox = context().parking().getMotorcycleBox(targetID);
        Assert(pBox != NULL);

        zoneid = pBox->getZone()->getZoneID();
        x = pBox->getX();
        y = pBox->getY();

        bFound = true;
    }

    return bFound;
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionSearchMotorcycle::toString() const

{
    StringStream msg;
    msg << "ActionSearchMotorcycle(" << ")";
    return msg.toString();
}
