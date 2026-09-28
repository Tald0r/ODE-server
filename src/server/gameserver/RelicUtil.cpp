#include "RelicUtil.h"

#include <stdio.h>

#include "BloodBible.h"
#include "CastleInfoManager.h"
#include "CastleSymbol.h"
#include "CombatInfoManager.h"
#include "Corpse.h"
#include "Creature.h"
#include "Effect.h"
#include "EffectDragonEye.h"
#include "EffectHasBloodBible.h"
#include "EffectHasCastleSymbol.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectRelicLock.h"
#include "EffectRelicPosition.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCDeleteInventoryItem.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "HolyLandManager.h"
#include "Inventory.h"
#include "ItemInfoManager.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "StringPool.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"
#include "war/DragonEyeManager.h"
#include "war/WarZoneRouting.h"

void sendBloodBibleEffect(Object* pObject, Effect::EffectClass EClass)

{
    __BEGIN_TRY

    Assert(pObject != NULL);

    switch (pObject->getObjectClass()) {
    case Object::OBJECT_CLASS_CREATURE: {
        Creature* pCreature = dynamic_cast<Creature*>(pObject);
        Assert(pCreature != NULL);

        GCAddEffect gcAddEffect;
        gcAddEffect.setEffectID(EClass);
        gcAddEffect.setObjectID(pCreature->getObjectID());

        pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
    } break;

    case Object::OBJECT_CLASS_ITEM: {
        Item* pItem = dynamic_cast<Item*>(pObject);
        Assert(pItem != NULL);

        if (pItem->getItemClass() == Item::ITEM_CLASS_CORPSE) {
            Corpse* pCorpse = dynamic_cast<Corpse*>(pItem);
            Assert(pCorpse != NULL);

            GCAddEffect gcAddEffect;
            gcAddEffect.setEffectID(EClass);
            gcAddEffect.setObjectID(pCorpse->getObjectID());

            pCorpse->getZone()->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcAddEffect);
        }
    } break;

    default:
        return;
    }

    __END_CATCH
}


// Timor southeast
//     Slayer ( 37, 163 )
//     Vampire ( 193, 46 )
//
// Adam's holy land, east <Vampire> ( 239, 134)
// Adam's holy land, west <Slayer> ( 27, 133 )
void sendHolyLandWarpEffect(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    bool bSend = false;
    Distance_t limitDist = 15;
    Effect::EffectClass EClass;

    ZoneID_t zoneID = pCreature->getZoneID();

    ZoneCoord_t x, y;

    if (pCreature->isSlayer()) {
        const int maxHolyLandWarpSlayer = 1;
        static ZONE_COORD HolyLandWarpSlayer[maxHolyLandWarpSlayer] = {
            ZONE_COORD(53, 37, 163), // Timor southeast
                                     //   ZONE_COORD( 73, 27, 133 )  // Adam's holy land, west
        };

        for (int i = 0; i < maxHolyLandWarpSlayer; i++) {
            const ZONE_COORD& zoneCoord = HolyLandWarpSlayer[i];
            x = zoneCoord.x;
            y = zoneCoord.y;

            if (zoneID == zoneCoord.id && pCreature->getDistance(x, y) < limitDist) {
                bSend = true;
                break;
            }
        }

        EClass = Effect::EFFECT_CLASS_WARP_HOLY_LAND_SLAYER;
    } else if (pCreature->isVampire()) {
        const int maxHolyLandWarpVampire = 1;
        static ZONE_COORD HolyLandWarpVampire[maxHolyLandWarpVampire] = {
            ZONE_COORD(53, 193, 46), // Timor southeast
                                     //   ZONE_COORD( 71, 239, 134 )  // Adam's holy land, east
        };

        for (int i = 0; i < maxHolyLandWarpVampire; i++) {
            const ZONE_COORD& zoneCoord = HolyLandWarpVampire[i];
            x = zoneCoord.x;
            y = zoneCoord.y;

            if (zoneID == zoneCoord.id && pCreature->getDistance(x, y) < limitDist) {
                bSend = true;
                break;
            }
        }

        EClass = Effect::EFFECT_CLASS_WARP_HOLY_LAND_VAMPIRE;
    } else if (pCreature->isOusters()) {
        // This still has to be done some day.
        // Where in Adam's holy land should Ousters be dropped?
        const int maxHolyLandWarpOusters = 1;
        static ZONE_COORD HolyLandWarpOusters[maxHolyLandWarpOusters] = {
            ZONE_COORD(53, 160, 170), // Timor southeast
                                      //   ZONE_COORD( 72, 129, 112 )  // Adam's holy land, center
        };

        for (int i = 0; i < maxHolyLandWarpOusters; i++) {
            const ZONE_COORD& zoneCoord = HolyLandWarpOusters[i];
            x = zoneCoord.x;
            y = zoneCoord.y;

            if (zoneID == zoneCoord.id && pCreature->getDistance(x, y) < limitDist) {
                bSend = true;
                break;
            }
        }

        EClass = Effect::EFFECT_CLASS_WARP_HOLY_LAND_OUSTERS;
    }

    if (bSend) {
        GCAddEffectToTile gcAddEffectToTile;
        gcAddEffectToTile.setEffectID(EClass);
        gcAddEffectToTile.setObjectID(pCreature->getObjectID());
        gcAddEffectToTile.setXY(x, y);
        gcAddEffectToTile.setDuration(21);

        pCreature->getZone()->broadcastPacket(x, y, &gcAddEffectToTile);
    }

    __END_CATCH
}

bool addEffectRelicPosition(Item* pItem, ZoneID_t zoneID, TPOINT pt)

{
    __BEGIN_TRY

    if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_POSITION)) {
        EffectRelicPosition* pPosition = new EffectRelicPosition(pItem);
        // pPosition->setNextTime(10);     // broadcast the message after 1 second
        pPosition->setTick(1 * 60 * 10); // announce once a minute
        pPosition->setZoneID(zoneID);
        pPosition->setX(pt.x);
        pPosition->setY(pt.y);
        pPosition->setPart(pItem->getItemType());
        pItem->setFlag(Effect::EFFECT_CLASS_RELIC_POSITION);
        pItem->getEffectManager().addEffect(pPosition);
        pPosition->affect();

        return true;
    }

    return false;

    __END_CATCH
}

bool deleteEffectRelicPosition(Item* pItem)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    // Remove the EffectRelicPosition.
    // Remove the effect attached to the relic table.
    if (pItem->isFlag(Effect::EFFECT_CLASS_RELIC_POSITION)) {
        Effect* pPositionEffect = pItem->getEffectManager().findEffect(Effect::EFFECT_CLASS_RELIC_POSITION);
        Assert(pPositionEffect != NULL);

        pPositionEffect->unaffect();
        pItem->removeFlag(Effect::EFFECT_CLASS_RELIC_POSITION);
        pItem->getEffectManager().deleteEffect(Effect::EFFECT_CLASS_RELIC_POSITION);

        return true;
    }

    return false;

    __END_CATCH
}

// Removes the effects related to pItem attached to the corpse.
bool deleteRelicEffect(Corpse* pCorpse, Item* pItem)

{
    __BEGIN_TRY

    Assert(pCorpse != NULL);
    Assert(pItem != NULL);

    Effect::EffectClass EClass;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_RELIC: {
        const RelicInfo* pRelicInfo = dynamic_cast<RelicInfo*>(
            de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, pItem->getItemType()));
        Assert(pRelicInfo != NULL);

        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER)
            EClass = Effect::EFFECT_CLASS_HAS_SLAYER_RELIC;
        else if (pRelicInfo->relicType == RELIC_TYPE_VAMPIRE)
            EClass = Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC;
        else
            throw Error("Invalid relic item type.");
    } break;

    case Item::ITEM_CLASS_BLOOD_BIBLE:
        EClass = Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE;
        break;

    case Item::ITEM_CLASS_CASTLE_SYMBOL:
        EClass = Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL;
        break;

    case Item::ITEM_CLASS_WAR_ITEM:
        EClass = Effect::EFFECT_CLASS_DRAGON_EYE;

    default:
        return false;
    }

    pCorpse->removeFlag(EClass);

    EffectManager& effectManager = pCorpse->getEffectManager();
    Effect* pEffect = effectManager.findEffect(EClass);
    if (pEffect != NULL) {
        pEffect->unaffect();
        effectManager.deleteEffect(EClass);
    }

    return true;

    __END_CATCH
}

void saveItemInCorpse(Item* pItem, Corpse* pCorpse)

{
    __BEGIN_TRY

    Assert(pItem != NULL);
    Assert(pCorpse != NULL);

    if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE ||
        pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL || pItem->isFlagItem() ||
        pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
        Zone* pZone = pCorpse->getZone();
        Assert(pZone != NULL);

        char pField[80];

        pZone->registerObject(pItem);

        snprintf(pField, sizeof(pField), "ObjectID = %u, OwnerID='%d', Storage=%d, StorageID=%u", pItem->getObjectID(),
                 (int)pZone->getZoneID(), (int)STORAGE_CORPSE, pCorpse->getObjectID());

        pItem->tinysave(pField);
    }

    __END_CATCH
}

bool isRelicItem(const Item* pItem) {
    if (pItem != NULL) {
        Item::ItemClass IClass = pItem->getItemClass();

        return isRelicItem(IClass);
    }

    return false;
}

void logRelicStorage(const Item* pItem, Storage storage, const string& ownerID) {
    if (de::war::relicMayLieIn((int)storage))
        return;

    filelog("WarError.log", "relic %u of class %u written to storage %d (owner '%s'), where no relic may lie",
            (unsigned)pItem->getItemID(), (unsigned)pItem->getItemClass(), (int)storage, ownerID.c_str());
}

bool isRelicItem(Item::ItemClass IClass) {
    if (IClass == Item::ITEM_CLASS_RELIC || IClass == Item::ITEM_CLASS_BLOOD_BIBLE ||
        IClass == Item::ITEM_CLASS_CASTLE_SYMBOL || IClass == Item::ITEM_CLASS_WAR_ITEM) {
        return true;
    }

    return false;
}

// pCorpse in the zone holds pItem.
bool addHasRelicEffect(Zone* pZone, Corpse* pCorpse, Item* pItem)

{
    __BEGIN_TRY

    if (pZone == NULL)
        return false;

    Assert(pCorpse != NULL);
    Assert(pItem != NULL);

    EffectHasRelic* pRelicEffect = NULL;

    // Marks that the altar holds the bible.
    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_BLOOD_BIBLE: {
        pRelicEffect = new EffectHasBloodBible(pCorpse);
    } break;

    case Item::ITEM_CLASS_CASTLE_SYMBOL: {
        pRelicEffect = new EffectHasCastleSymbol(pCorpse);
    } break;

    default:
        return false;
    }

    pRelicEffect->setZone(pZone);
    pRelicEffect->setXY(pCorpse->getX(), pCorpse->getY());
    // pRelicEffect->setNextTime( 1*10 );   // after 1 second
    pRelicEffect->setTick(1 * 60 * 10); // print the message once a minute
    pRelicEffect->setPart(pItem->getItemType());

    pRelicEffect->affect();

    EffectManager& effectManager = pCorpse->getEffectManager();
    pCorpse->setFlag(pRelicEffect->getEffectClass());
    effectManager.addEffect(pRelicEffect);

    // Tell the client to attach the effect.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCorpse->getObjectID());
    gcAddEffect.setEffectID(pRelicEffect->getSendEffectClass());
    gcAddEffect.setDuration(65000);
    pZone->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcAddEffect);

    return true;

    __END_CATCH
}


bool deleteRelicEffect(Creature* pCreature, Item* pItem)

{
    __BEGIN_TRY

    Assert(pItem != NULL);
    Assert(pCreature != NULL);

    Effect::EffectClass effectClass;

    if (pItem->getItemClass() == Item::ITEM_CLASS_RELIC) {
        const RelicInfo* pRelicInfo = dynamic_cast<RelicInfo*>(
            de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, pItem->getItemType()));
        Assert(pRelicInfo != NULL);

        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER)
            effectClass = Effect::EFFECT_CLASS_HAS_SLAYER_RELIC;
        else if (pRelicInfo->relicType == RELIC_TYPE_VAMPIRE)
            effectClass = Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC;
        else
            throw Error("Invalid relic item type.");
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_BLOOD_BIBLE) {
        effectClass = Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE;
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        effectClass = Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL;
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_WAR_ITEM) {
        effectClass = Effect::EFFECT_CLASS_DRAGON_EYE;
    } else {
        return false;
    }

    // Find the has-relic effect and remove it.
    Effect* pEffect = pCreature->findEffect(effectClass);
    if (pEffect != NULL) {
        pCreature->removeFlag(effectClass);
        pEffect->unaffect();
        pCreature->deleteEffect(effectClass);

        return true;
    }

    return false;

    __END_CATCH
}

bool addRelicEffect(Creature* pCreature, Item* pItem)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    Assert(pItem != NULL);

    // Attach the effect that marks holding a relic.
    Effect::EffectClass effectClass;
    Effect::EffectClass effectClassSend;

    Item::ItemClass itemclass = pItem->getItemClass();
    ItemType_t itemtype = pItem->getItemType();

    if (itemclass == Item::ITEM_CLASS_RELIC) {
        const RelicInfo* pRelicInfo =
            dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, itemtype));

        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER) {
            effectClassSend = effectClass = Effect::EFFECT_CLASS_HAS_SLAYER_RELIC;
            EffectHasRelic* pEffect = new EffectHasSlayerRelic(pCreature);
            // pEffect->setNextTime( 1*10 );  // after 10 seconds
            pEffect->setTick(1 * 60 * 10); // print the message once a minute
            pCreature->addEffect(pEffect);
            pEffect->affect();
        } else {
            effectClassSend = effectClass = Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC;
            EffectHasRelic* pEffect = new EffectHasVampireRelic(pCreature);
            // pEffect->setNextTime( 1*10 );  // after 10 seconds
            pEffect->setTick(1 * 60 * 10); // print the message once a minute
            pCreature->addEffect(pEffect);
            pEffect->affect();
        }
    } else if (itemclass == Item::ITEM_CLASS_BLOOD_BIBLE) {
        effectClass = Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE;
        effectClassSend = (Effect::EffectClass)((int)Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE + itemtype);
        EffectHasRelic* pEffect = new EffectHasBloodBible(pCreature);
        // pEffect->setNextTime( 1*10 );  // after 10 seconds
        pEffect->setTick(1 * 60 * 10); // print the message once a minute
        pEffect->setPart(itemtype);
        pCreature->addEffect(pEffect);
        pEffect->affect();
    } else if (itemclass == Item::ITEM_CLASS_CASTLE_SYMBOL) {
        effectClass = Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL;
        effectClassSend = (Effect::EffectClass)((int)Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL + itemtype);
        EffectHasRelic* pEffect = new EffectHasCastleSymbol(pCreature);
        // pEffect->setNextTime( 1*10 );  // after 10 seconds
        pEffect->setTick(1 * 60 * 10); // print the message once a minute
        pEffect->setPart(itemtype);
        pCreature->addEffect(pEffect);
        pEffect->affect();
    } else if (itemclass == Item::ITEM_CLASS_WAR_ITEM) {
        effectClass = Effect::EFFECT_CLASS_DRAGON_EYE;
        effectClassSend = effectClass;
        EffectHasRelic* pEffect = new EffectDragonEye(pCreature);
        EffectDragonEye* pDragonEyeEffect = dynamic_cast<EffectDragonEye*>(pEffect);
        pDragonEyeEffect->setItemID(pItem->getItemID());
        pEffect->setTick(999999);
        pEffect->setPart(0);
        pCreature->setFlag(effectClass);
        pCreature->addEffect(pEffect);
        pEffect->affect();
    } else {
        return false;
    }

    pCreature->setFlag(effectClass);

    // Announce that the effect was attached.
    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(effectClassSend);
    gcAddEffect.setDuration(65000);
    pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);

    return true;

    __END_CATCH
}


bool dropRelicToZone(PlayerCreature* pPC, Item* pItem)

{
    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    // Drop the item on the ground, on a square with no character on it
    // since it could otherwise overlap a corpse.
    TPOINT pt = pZone->addItem(pItem, pPC->getX(), pPC->getY(), false);

    if (pt.x != -1) // if the drop succeeded
    {
        char pField[80];
        snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                 pZone->getZoneID(), pt.x, pt.y);
        pItem->tinysave(pField);

        // Take it out of the inventory.
        // pInventory->deleteItem( pItem->getObjectID() );
        deleteRelicEffect(pPC, pItem);

        // Announce from time to time where the relic was dropped.


        // Send every player a message saying the relic was dropped.
        if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
            EffectRelicLock* pLock = new EffectRelicLock(pItem);
            pLock->setDeadline(10 * 10); // 10 seconds
            pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
            pItem->getEffectManager().addEffect(pLock);
        }
    } else {
        throw Error("No free tile to drop the relic on logout");
        // return false;
    }

    return true;
}


bool dropRelicToZone(Creature* pCreature, bool bSendPacket)

{
    __BEGIN_TRY

    bool bDrop = false;

    ///////////////////////////////////////////////////////////////////
    // If the creature holds a DragonEye when it dies, it goes back to its
    // original position. A relic carried beside it -- a plain relic may be,
    // since holding one does not stop a dragon eye's pickup -- still drops
    // below.
    ///////////////////////////////////////////////////////////////////
    if (pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE)) {
        de::gameContext().dragonEyes().warpToDefaultPosition(pCreature);

        Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_DRAGON_EYE);
        if (pEffect != NULL) {
            pCreature->removeFlag(Effect::EFFECT_CLASS_DRAGON_EYE);
            pEffect->unaffect();
            pCreature->deleteEffect(Effect::EFFECT_CLASS_DRAGON_EYE);
        }
        bDrop = true;
    }

    ///////////////////////////////////////////////////////////////////
    // If the creature holds a relic item when it dies, drop it on the ground.
    ///////////////////////////////////////////////////////////////////
    if (pCreature->hasRelicItem()) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        // Check whether a relic is held on the mouse cursor.
        Item* pSlotItem = pPC->getExtraInventorySlotItem();

        if (pSlotItem != NULL && isRelicItem(pSlotItem)) {
            if (dropRelicToZone(pPC, pSlotItem)) {
                pPC->deleteItemFromExtraInventorySlot();

                // Remove it from the player's mouse cursor.
                // When the client receives this packet it also
                // checks the mouse cursor.

                if (bSendPacket) {
                    GCDeleteInventoryItem gcDeleteInventoryItem;
                    gcDeleteInventoryItem.setObjectID(pSlotItem->getObjectID());

                    pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
                }

                bDrop = true;
            }
        }

        Zone* pZone = pPC->getZone();
        Assert(pZone != NULL);

        Inventory* pInventory = pPC->getInventory();
        Assert(pInventory != NULL);

        ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(pZone->getZoneID());
        Assert(pZoneInfo != NULL);

        // Look for a relic item in the inventory.
        for (CoordInven_t y = 0; y < pInventory->getHeight(); y++) {
            for (CoordInven_t x = 0; x < pInventory->getWidth(); x++) {
                Item* pItem = pInventory->getItem(x, y);
                if (pItem != NULL && isRelicItem(pItem)) {
                    // Drop the item on the ground.
                    if (dropRelicToZone(pPC, pItem)) {
                        // Take it out of the inventory.
                        pInventory->deleteItem(pItem->getObjectID());

                        // Remove it from the player's inventory.
                        if (bSendPacket) {
                            GCDeleteInventoryItem gcDeleteInventoryItem;
                            gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

                            pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
                        }

                        bDrop = true;
                    }
                }
            }
        }
    }

    return bDrop;

    __END_CATCH
}


// Handles a relic item coming out of a corpse.
bool dissectionRelicItem(Corpse* pCorpse, Item* pItem, const TPOINT& pt)

{
    __BEGIN_TRY

    StringPool& strings = de::gameContext().strings();

    if (!isRelicItem(pItem))
        return false;

    switch (pItem->getItemClass()) {
    //----------------------------------------------------------------------
    //
    // 							Relic
    //
    //----------------------------------------------------------------------
    case Item::ITEM_CLASS_RELIC: {
        // If no item is left (it was the last one), the EffectRelic is deleted.
        try {
            int relicIndex = pItem->getItemType();
            const RelicInfo* pRelicInfo = dynamic_cast<RelicInfo*>(
                de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, pItem->getItemType()));

            deleteRelicEffect(pCorpse, pItem);

            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_NULL);

            // Room for the longest message a GCSystemMessage carries.
            char msg[de::wire::kMaxByteStringLength + 1];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_RELIC_FROM_RELIC_TABLE), pRelicInfo->getName().c_str());

            //				StringStream msg;
            //        msg << "Out of the relic table, "
            //          << "the relic (" << pRelicInfo->getName() << ") came out.";

            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            de::gameContext().zoneGroups().broadcast(&gcSystemMessage);


            // The relic left the relic table, so the
            // bonuses and penalties are recomputed.
            de::gameContext().combatInfo().computeModify();
        } catch (Throwable& t) {
            cout << t.toString().c_str() << endl;
            throw;
        }
    }
        return true;

    //----------------------------------------------------------------------
    //
    // 							BloodBible
    //
    //----------------------------------------------------------------------
    case Item::ITEM_CLASS_BLOOD_BIBLE: {
        // deleteRelicEffect( pCorpse, pItem );

        const BloodBibleInfo* pBloodBibleInfo = dynamic_cast<BloodBibleInfo*>(
            de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, pItem->getItemType()));

        //			StringStream msg;
        //      msg << "A blood bible fragment (" << pBloodBibleInfo->getName() << ") came out.";

        char msg[200];
        snprintf(msg, sizeof(msg), strings.c_str(STRID_BLOOD_BIBLE_FROM_SHRINE), pBloodBibleInfo->getName().c_str());

        GCSystemMessage gcSystemMessage;
        gcSystemMessage.setMessage(msg);
        // g_pZoneGroupManager->broadcast( &gcSystemMessage );
        de::gameContext().holyLands().broadcast(&gcSystemMessage);

        // Update the bible fragment item's position.
        if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_POSITION)) {
            EffectRelicPosition* pPosition = new EffectRelicPosition(pItem);
            // pPosition->setNextTime(10);     // broadcast the message after 1 second
            pPosition->setTick(1 * 60 * 10); // announce once a minute
            pPosition->setZoneID(pCorpse->getZone()->getZoneID());
            pPosition->setX(pt.x);
            pPosition->setY(pt.y);
            pPosition->setPart(pItem->getItemType());
            pItem->setFlag(Effect::EFFECT_CLASS_RELIC_POSITION);
            pItem->getEffectManager().addEffect(pPosition);
            pPosition->affect();
        }
    }
        return true;

    //----------------------------------------------------------------------
    //
    // 							CastleSymbol
    //
    //----------------------------------------------------------------------
    case Item::ITEM_CLASS_CASTLE_SYMBOL: {
        // deleteRelicEffect( pCorpse, pItem );

        const CastleSymbolInfo* pCastleSymbolInfo = dynamic_cast<const CastleSymbolInfo*>(
            de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_CASTLE_SYMBOL, pItem->getItemType()));

        if (pCastleSymbolInfo != NULL) {
            //				StringStream msg;
            //        msg << "A castle symbol (" << pCastleSymbolInfo->getName() << ") came out.";

            char msg[200];
            snprintf(msg, sizeof(msg), strings.c_str(STRID_CASTLE_SYMBOL_FROM_SHRINE),
                     pCastleSymbolInfo->getName().c_str());
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(msg);
            // g_pZoneGroupManager->broadcast( &gcSystemMessage );
            de::gameContext().castleInfos().broadcastShrinePacket(pItem->getItemType(), &gcSystemMessage);


            // Update the bible fragment item's position.
            if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_POSITION)) {
                EffectRelicPosition* pPosition = new EffectRelicPosition(pItem);
                // pPosition->setNextTime(10);     // broadcast the message after 1 second
                pPosition->setTick(1 * 60 * 10); // announce once a minute
                pPosition->setZoneID(pCorpse->getZone()->getZoneID());
                pPosition->setX(pt.x);
                pPosition->setY(pt.y);
                pPosition->setPart(pItem->getItemType());
                pItem->setFlag(Effect::EFFECT_CLASS_RELIC_POSITION);
                pItem->getEffectManager().addEffect(pPosition);
                pPosition->affect();
            }
        }
    }
        return true;


    default:
        return false;
    }

    return false;

    __END_CATCH
}

// When a relic warps away from pCorpse,
// attach the effect to pCorpse.
void sendRelicWarpEffect(Corpse* pCorpse)

{
    __BEGIN_TRY

    Assert(pCorpse != NULL);

    if (pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_GUARD)) {
        sendBloodBibleEffect(pCorpse, Effect::EFFECT_CLASS_SHRINE_GUARD_WARP);
    } else if (pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_HOLY)) {
        sendBloodBibleEffect(pCorpse, Effect::EFFECT_CLASS_SHRINE_HOLY_WARP);
    } else {
        ZoneID_t relicZoneID = pCorpse->getZone()->getZoneID();
        ZoneID_t castleZoneID;

        bool isCastle = de::gameContext().castleInfos().getCastleZoneID(relicZoneID, castleZoneID);

        if (!isCastle)
            return;

        CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(castleZoneID);
        Assert(pCastleInfo != NULL);

        if (pCastleInfo->getRace() == RACE_SLAYER) {
            sendBloodBibleEffect(pCorpse, Effect::EFFECT_CLASS_CASTLE_SHRINE_SLAYER_WARP);
        } else {
            sendBloodBibleEffect(pCorpse, Effect::EFFECT_CLASS_CASTLE_SHRINE_VAMPIRE_WARP);
        }
    }

    __END_CATCH
}
