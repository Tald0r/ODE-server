///////////////////////////////////////////////////////////////////////////
// File : DragonEyeManager.cpp
///////////////////////////////////////////////////////////////////////////

// include files
#include "DragonEyeManager.h"

#include <stdio.h>

#include "EffectDragonEye.h"
#include "GameContext.h"
#include "GlobalItemPositionLoader.h"
#include "ItemFactoryManager.h"
#include "WarZoneWork.h"
#include "Zone.h"
#include "ZoneUtil.h"

const int nDragonEyes = 9;

///////////////////////////////////////////////////////////////////////////
// class DragonEyeManager
///////////////////////////////////////////////////////////////////////////
DragonEyeManager::DragonEyeManager() {
    __BEGIN_TRY

    m_DragonEyes.resize(nDragonEyes, NULL);
    m_DefaultPositions.resize(nDragonEyes);

    initDefaultPositions();

    __END_CATCH
}

DragonEyeManager::~DragonEyeManager() {}

void DragonEyeManager::init()

{
    __BEGIN_TRY

    createAllDragonEyes();

    __END_CATCH
}

void DragonEyeManager::initDefaultPositions()

{
    __BEGIN_TRY

    // Set the DragonEye starting positions when the war begins.
    m_DefaultPositions[0].set(73, 24, 121);  // Slayer / Adam West / 24 / 121
    m_DefaultPositions[1].set(1201, 62, 68); // Slayer / Octavus / 62 / 68
    m_DefaultPositions[2].set(1203, 85, 50); // Slayer / Septimus / 85 / 50
    m_DefaultPositions[3].set(71, 111, 124); // Vampire / Adam East / 111 / 124
    m_DefaultPositions[4].set(1204, 62, 69); // Vampire / Quartus / 62 / 69
    m_DefaultPositions[5].set(1202, 81, 52); // Vampire / Tertius / 81 / 52
    m_DefaultPositions[6].set(72, 66, 168);  // Ousters / Adam / 66 / 168
    m_DefaultPositions[7].set(1205, 62, 69); // Ousters / Pentanus / 62 / 69
    m_DefaultPositions[8].set(1206, 82, 52); // Ousters / Hexirius / 82 / 52

    __END_CATCH
}

void DragonEyeManager::createAllDragonEyes()

{
    __BEGIN_TRY

    // First create the dragon eye items.
    for (int i = 0; i < nDragonEyes; ++i) {
        list<OptionType_t> optionNull;
        Item* pItem = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_WAR_ITEM, 0, optionNull);
        Assert(pItem != NULL);

        WarItem* pWarItem = dynamic_cast<WarItem*>(pItem);
        Assert(pWarItem != NULL);

        Zone* pZone = getZoneByZoneID(m_DefaultPositions[i].id);
        Assert(pZone != NULL);

        char strZoneID[10];
        snprintf(strZoneID, sizeof(strZoneID), "%u", pZone->getZoneID());

        pZone->registerObject(pWarItem);
        pWarItem->create(strZoneID, STORAGE_ZONE, pZone->getZoneID(), m_DefaultPositions[i].x, m_DefaultPositions[i].y);

        pWarItem->setIndex(i);

        m_DragonEyes[i] = pWarItem;
    }

    __END_CATCH
}

void DragonEyeManager::addAllDragonEyesToZone()

{
    __BEGIN_TRY

    // Put each item at its initial position.
    for (int i = 0; i < nDragonEyes; ++i) {
        Zone* pZone = getZoneByZoneID(m_DefaultPositions[i].id);
        Assert(pZone != NULL);

        Item* pItem = m_DragonEyes[i];
        Assert(pItem != NULL);

        // Add the item to the zone
        pZone->addItemDelayed(pItem, m_DefaultPositions[i].x, m_DefaultPositions[i].y);
    }

    __END_CATCH
}

void DragonEyeManager::removeAllDragonEyes()

{
    __BEGIN_TRY

    // Take every dragon eye out of the world so that it cannot be seen. Each
    // is taken by whoever holds it, on that holder's thread; the item itself
    // stays here, in m_DragonEyes, for the next race war to put back.
    for (int i = 0; i < nDragonEyes; ++i) {
        de::war::postItemReturn(Item::ITEM_CLASS_WAR_ITEM, m_DragonEyes[i]->getItemID(), [](Zone&, Item*) {});
    }

    __END_CATCH
}

void DragonEyeManager::warpToDefaultPosition(Creature* pCreature)

{
    __BEGIN_TRY

    // pCreature is the creature that holds the dragon eye.
    // Called when the user that held the dragon eye dies or leaves.
    // This is the code that moves the dragon eye back to its original position.
    // GlobalItemPositionLoader is used to take the item off its current position.
    // Take the original zone's pointer and put the item in.
    // This function is called when a user acts, so it runs on the ZoneGroup.
    // So when working with a zone pointer, the ZoneGroup is compared first.
    EffectDragonEye* pEffect =
        dynamic_cast<EffectDragonEye*>(pCreature->getEffectManager()->findEffect(Effect::EFFECT_CLASS_DRAGON_EYE));
    Assert(pEffect != NULL);

    Item::ItemClass ItemClass = Item::ITEM_CLASS_WAR_ITEM;
    ItemID_t ItemID = pEffect->getItemID();

    GlobalItemPosition* pItemPosition = GlobalItemPositionLoader::getInstance()->load(ItemClass, ItemID);

    if (pItemPosition == NULL)
        return;

    Item* pItem = pItemPosition->popItem(false);

    if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_WAR_ITEM) {
        WarItem* pWarItem = dynamic_cast<WarItem*>(pItem);
        Assert(pWarItem != NULL);

        int index = pWarItem->getIndex();

        Zone* pZone = pCreature->getZone();
        Assert(pZone != NULL);

        Zone* pTargetZone = getZoneByZoneID(m_DefaultPositions[index].id);
        Assert(pTargetZone != NULL);

        if (pZone->getZoneGroup() == pTargetZone->getZoneGroup()) {
            pTargetZone->addItem(pItem, m_DefaultPositions[index].x, m_DefaultPositions[index].y);
        } else {
            // The other group adds the eye on its own tick and saves where it
            // lands. Until then the row names the default tile rather than
            // the holder, who may be logging out, so a war's return that
            // reads it goes to the group the eye is going to.
            char pField[80];
            snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                     (unsigned)pTargetZone->getZoneID(), (int)m_DefaultPositions[index].x,
                     (int)m_DefaultPositions[index].y);
            pWarItem->tinysave(pField);

            pTargetZone->addItemDelayed(pItem, m_DefaultPositions[index].x, m_DefaultPositions[index].y);
        }
    }

    __END_CATCH
}
