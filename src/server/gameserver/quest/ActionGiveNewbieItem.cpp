////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionGiveNewbieItem.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionGiveNewbieItem.h"

#include "FlagSet.h"
#include "GCCreateItem.h"
#include "GCModifyInformation.h"
#include "GCNPCResponse.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "Slayer.h"
#include "Zone.h"
#include "item/Magazine.h"

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionGiveNewbieItem::read(PropertyBuffer& propertyBuffer)

{
    try {
        // read script type
        string ItemType = propertyBuffer.getProperty("Type");

        if (ItemType == "SWORD")
            m_ItemClass = Item::ITEM_CLASS_SWORD;
        else if (ItemType == "BLADE")
            m_ItemClass = Item::ITEM_CLASS_BLADE;
        else if (ItemType == "AR")
            m_ItemClass = Item::ITEM_CLASS_AR;
        else if (ItemType == "SR")
            m_ItemClass = Item::ITEM_CLASS_SR;
        else if (ItemType == "SG")
            m_ItemClass = Item::ITEM_CLASS_SG;
        else if (ItemType == "SMG")
            m_ItemClass = Item::ITEM_CLASS_SMG;
        else if (ItemType == "CROSS")
            m_ItemClass = Item::ITEM_CLASS_CROSS;
        else if (ItemType == "MACE")
            m_ItemClass = Item::ITEM_CLASS_MACE;
        else {
            cout << "ActionGiveNewbieItem::read() : Unknown item type" << endl;
            throw Error("ActionGiveNewbieItem::read() : Unknown item type");
        }
    } catch (NoSuchElementException& nsee) {
        cout << nsee.toString() << endl;
        throw Error(nsee.toString());
    }
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionGiveNewbieItem::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    Player* pPlayer = pCreature2->getPlayer();
    Assert(pPlayer != NULL);

    // Send GCNPCResponse to the client first.
    GCNPCResponse okpkt;
    pPlayer->sendPacket(&okpkt);

    // A newbie is always a Slayer.
    if (!pCreature2->isSlayer())
        return;

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
    FlagSet* pFlagSet = pSlayer->getFlagSet();
    Inventory* pInventory = pSlayer->getInventory();
    Zone* pZone = pSlayer->getZone();

    ////////////////////////////////////////////////////////////
    // Check that the character really is a newbie.
    // Flag 0 marks that the newbie items were already received.
    // This is a stopgap.
    ////////////////////////////////////////////////////////////

    if (pFlagSet->isOn(0))
        return;

    ////////////////////////////////////////////////////////////
    // Create the items for each item class.
    ////////////////////////////////////////////////////////////
    Item* pItem[10] = {
        NULL,
    };
    int i = 0;

    for (i = 0; i < 10; i++)
        pItem[i] = NULL;

    list<OptionType_t> nullList;

    ItemFactoryManager& itemFactories = context().itemFactories();

    if (m_ItemClass == Item::ITEM_CLASS_SWORD) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_SWORD, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
    } else if (m_ItemClass == Item::ITEM_CLASS_BLADE) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_BLADE, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
    } else if (m_ItemClass == Item::ITEM_CLASS_CROSS) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_CROSS, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
    } else if (m_ItemClass == Item::ITEM_CLASS_MACE) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_MACE, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
    } else if (m_ItemClass == Item::ITEM_CLASS_AR) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_AR, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        // Magazine
        pItem[5] = itemFactories.createItem(Item::ITEM_CLASS_MAGAZINE, 2, nullList);
        Magazine* pMagazine = dynamic_cast<Magazine*>(pItem[5]);
        pMagazine->setNum(9);
    } else if (m_ItemClass == Item::ITEM_CLASS_SR) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_SR, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        // Magazine
        pItem[5] = itemFactories.createItem(Item::ITEM_CLASS_MAGAZINE, 6, nullList);
        Magazine* pMagazine = dynamic_cast<Magazine*>(pItem[5]);
        pMagazine->setNum(9);
    } else if (m_ItemClass == Item::ITEM_CLASS_SG) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_SG, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        // Magazine
        pItem[5] = itemFactories.createItem(Item::ITEM_CLASS_MAGAZINE, 0, nullList);
        Magazine* pMagazine = dynamic_cast<Magazine*>(pItem[5]);
        pMagazine->setNum(9);
    } else if (m_ItemClass == Item::ITEM_CLASS_SMG) {
        // Weapon
        pItem[0] = itemFactories.createItem(Item::ITEM_CLASS_SMG, 0, nullList);
        // Potion
        pItem[1] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[2] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 0, nullList);
        pItem[3] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        pItem[4] = itemFactories.createItem(Item::ITEM_CLASS_POTION, 5, nullList);
        // Magazine
        pItem[5] = itemFactories.createItem(Item::ITEM_CLASS_MAGAZINE, 4, nullList);
        Magazine* pMagazine = dynamic_cast<Magazine*>(pItem[5]);
        pMagazine->setNum(9);
    } else {
        return;
    }

    ////////////////////////////////////////////////////////////
    // Register the created items, save them to the database and
    // send the information to the client.
    ////////////////////////////////////////////////////////////
    ObjectRegistry& OR = pZone->getObjectRegistry();
    TPOINT pt;
    GCCreateItem gcCreateItem;

    for (i = 0; i < 10; i++) {
        if (pItem[i] != NULL) {
            OR.registerObject(pItem[i]);

            if (pInventory->addItem(pItem[i], pt)) {
                pItem[i]->create(pSlayer->getName(), STORAGE_INVENTORY, 0, pt.x, pt.y);

                gcCreateItem.setObjectID(pItem[i]->getObjectID());
                gcCreateItem.setItemClass(pItem[i]->getItemClass());
                gcCreateItem.setItemType(pItem[i]->getItemType());
                gcCreateItem.setOptionType(pItem[i]->getOptionTypeList());
                gcCreateItem.setDurability(pItem[i]->getDurability());
                gcCreateItem.setSilver(pItem[i]->getSilver());

                if (pItem[i]->getItemClass() == Item::ITEM_CLASS_MAGAZINE) {
                    Magazine* pMag = dynamic_cast<Magazine*>(pItem[i]);
                    gcCreateItem.setItemNum(pMag->getNum());
                } else {
                    gcCreateItem.setItemNum(pItem[i]->getNum());
                }

                gcCreateItem.setInvenX(pt.x);
                gcCreateItem.setInvenY(pt.y);

                pPlayer->sendPacket(&gcCreateItem);
            } else {
                SAFE_DELETE(pItem[i]);
            }
        }
    }

    // Gold is handed out as well.
    // by sigi. 2002.9.18
    pSlayer->increaseGoldEx(500);
    GCModifyInformation gcModifyInformation;
    gcModifyInformation.addLongData(MODIFY_GOLD, pSlayer->getGold());
    pPlayer->sendPacket(&gcModifyInformation);

    ////////////////////////////////////////////////////////////
    // The items were handed out, so save the current FlagSet.
    // Flag 0 marks that the newbie items were already received.
    // This is a stopgap.
    ////////////////////////////////////////////////////////////
    pFlagSet->turnOn(FLAGSET_RECEIVE_NEWBIE_ITEM);

    if (m_ItemClass == Item::ITEM_CLASS_SWORD || m_ItemClass == Item::ITEM_CLASS_BLADE) {
        // Fighter: 1
        pFlagSet->turnOn(FLAGSET_RECEIVE_NEWBIE_ITEM_FIGHTER);
    } else if (m_ItemClass == Item::ITEM_CLASS_CROSS || m_ItemClass == Item::ITEM_CLASS_MACE) {
        // Cleric: 2
        pFlagSet->turnOn(FLAGSET_RECEIVE_NEWBIE_ITEM_CLERIC);
    } else {
        // Gunner: 3
        pFlagSet->turnOn(FLAGSET_RECEIVE_NEWBIE_ITEM_GUNNER);
    }

    pFlagSet->save(pSlayer->getName());
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionGiveNewbieItem::toString() const

{
    StringStream msg;
    msg << "ActionGiveNewbieItem(" << ")";
    return msg.toString();
}
