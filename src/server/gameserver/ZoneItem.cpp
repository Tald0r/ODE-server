//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneItem.cpp
// Description	: Zone item tables: dropping, moving and removing the items that lie on the ground.
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <fstream>

#include "Assert.h"
#include "BloodBibleBonusManager.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "Creature.h"
#include "DarkLightInfo.h"
#include "DefaultOptionSetInfo.h"
#include "DynamicZone.h"
#include "EffectAddItem.h"
#include "EffectAddItemToCorpse.h"
#include "EffectCastingTrap.h"
#include "EffectContinualGroundAttack.h"
#include "EffectDarkness.h"
#include "EffectDecayCorpse.h"
#include "EffectDecayItem.h"
#include "EffectDeleteItem.h"
#include "EffectGnomesWhisper.h"
#include "EffectHasBloodBible.h"
#include "EffectHasCastleSymbol.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectLoaderManager.h"
#include "EffectManager.h"
#include "EffectObservingEye.h"
#include "EffectPKZoneRegen.h"
#include "EffectRelicTable.h"
#include "EffectSanctuary.h"
#include "EffectSchedule.h"
#include "EffectShrineGuard.h"
#include "EffectShrineHoly.h"
#include "EffectShrineShield.h"
#include "EffectSlayerRelic.h"
#include "EffectTransportItem.h"
#include "EffectTransportItemToCorpse.h"
#include "EffectVampirePortal.h"
#include "EffectVampireRelic.h"
#include "EventTransport.h"
#include "FlagSet.h"
#include "GCAddBat.h"
#include "GCAddBurrowingCreature.h"
#include "GCAddEffect.h"
#include "GCAddEffectToTile.h"
#include "GCAddInstalledMineToZone.h"
#include "GCAddMonster.h"
#include "GCAddMonsterCorpse.h"
#include "GCAddMonsterFromBurrowing.h"
#include "GCAddMonsterFromTransformation.h"
#include "GCAddNPC.h"
#include "GCAddNewItemToZone.h"
#include "GCAddOusters.h"
#include "GCAddOustersCorpse.h"
#include "GCAddSlayer.h"
#include "GCAddSlayerCorpse.h"
#include "GCAddVampire.h"
#include "GCAddVampireCorpse.h"
#include "GCAddVampireFromBurrowing.h"
#include "GCAddVampireFromTransformation.h"
#include "GCAddVampirePortal.h"
#include "GCAddWolf.h"
#include "GCDeleteEffectFromTile.h"
#include "GCDeleteObject.h"
#include "GCDropItemToZone.h"
#include "GCFastMove.h"
#include "GCHolyLandBonusInfo.h"
#include "GCKnockBack.h"
#include "GCMineExplosionOK1.h"
#include "GCMineExplosionOK2.h"
#include "GCModifyInformation.h"
#include "GCMove.h"
#include "GCMoveError.h"
#include "GCMoveOK.h"
#include "GCMyStoreInfo.h"
#include "GCNPCInfo.h"
#include "GCNoticeEvent.h"
#include "GCRegenZoneStatus.h"
#include "GCRemoveEffect.h"
#include "GCSetPosition.h"
#include "GCSweeperBonusInfo.h"
#include "GCSystemMessage.h"
#include "GCUnburrowFail.h"
#include "GCUnburrowOK.h"
#include "GCUnionOfferList.h"
#include "GCUntransformFail.h"
#include "GCUntransformOK.h"
#include "GDRLairManager.h"
#include "GGCommand.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameServerInfoManager.h"
#include "GuildManager.h"
#include "GuildUnion.h"
#include "HolyLandManager.h"
#include "Item.h"
#include "ItemFactoryManager.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "LoginServerManager.h"
#include "MasterLairInfoManager.h"
#include "MasterLairManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterManager.h"
#include "NPC.h"
#include "NPCInfo.h"
#include "NPCManager.h"
#include "NicknameBook.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PCFinder.h"
#include "PCManager.h"
#include "PKZoneInfoManager.h"
#include "PacketUtil.h"
#include "ParkingCenter.h"
#include "Party.h"
#include "PaySystem.h"
#include "Player.h"
#include "Profile.h"
#include "Properties.h"
#include "QuestManager.h"
#include "RegenZoneManager.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ResurrectLocationManager.h"
#include "ShrineInfoManager.h"
#include "SiegeManager.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "Store.h"
#include "StringPool.h"
#include "SweeperBonusManager.h"
#include "TimeManager.h"
#include "TradeManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "VariableManager.h"
#include "VisionInfo.h"
#include "War.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "WeatherManager.h"
#include "Zone.h"
#include "ZoneGroup.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneInternal.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "item/Motorcycle.h"
#include "item/VampirePortalItem.h"
#include "repository/ComebackEventRepository.h"
#include "repository/MessageRepository.h"
#include "repository/ZoneInfoRepository.h"


#ifdef __PROFILE_BROADCAST__
#define __BEGIN_PROFILE_ZONE(name) beginProfileEx(name);
#define __END_PROFILE_ZONE(name) endProfileEx(name);
#else
#define __BEGIN_PROFILE_ZONE(name) ((void)0);
#define __END_PROFILE_ZONE(name) ((void)0);
#endif


#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif

// Time before corpses/items on the ground disappear in the master lair.
const Turn_t DELAY_MASTER_LAIR_DECAY_CORPSE = 200;       // 20 seconds
const Turn_t DELAY_MASTER_LAIR_DECAY_ITEM = 400;         // 40 seconds
const Turn_t DELAY_MASTER_LAIR_DECAY_MASTER_CORPSE = 50; // 5 seconds

//--------------------------------------------------------------------------------
// Drop an item at a given position.
// Zone ::addItem()
// Scans the 7x7 area and drops the item on a free square. If no free square
// exists, it answers (-1, -1) and the caller handles that case.
//--------------------------------------------------------------------------------
TPOINT Zone::addItem(Item* pItem, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature, Turn_t decayTurn,
                     ObjectID_t DropPetOID)

{
    __BEGIN_TRY

    __BEGIN_DEBUG

    TPOINT pt;

    __BEGIN_PROFILE_ZONE("Z_ADD_ITEM")

    Item::ItemClass IClass = pItem->getItemClass();

    bool bAllowSafeZone = true;

    if (isRelicItem(IClass) || pItem->isFlagItem())
        bAllowSafeZone = false;

    bool bDropForce = false;
    if (pItem->isFlag(Effect::EFFECT_CLASS_DROP_FORCE)) {
        pItem->removeFlag(Effect::EFFECT_CLASS_DROP_FORCE);
        bDropForce = true;
    }
    pt = findSuitablePositionForItem(this, cx, cy, bAllowCreature, bAllowSafeZone, bDropForce);

    // A place to put it was found.
    if (pt.x != -1) {
        m_pTiles[pt.x][pt.y].addItem(pItem);
        addToItemList(pItem);

        if (IClass == Item::ITEM_CLASS_CORPSE) {
            ItemType_t itemType = pItem->getItemType();

            Turn_t DelayTime = 0;

            bool isShrine = false;
            bool isFlag = false;

            if (itemType == SLAYER_CORPSE) {
                SlayerCorpse* pSlayerCorpse = dynamic_cast<SlayerCorpse*>(pItem);
                pSlayerCorpse->setXY(pt.x, pt.y);

                GCAddSlayerCorpse gcAddSlayerCorpse;
                makeGCAddSlayerCorpse(&gcAddSlayerCorpse, pSlayerCorpse);
                broadcastPacket(pt.x, pt.y, &gcAddSlayerCorpse);

                // Corpses disappear faster in the master lair.
                if (isMasterLair())
                    DelayTime = DELAY_MASTER_LAIR_DECAY_CORPSE;
                else
                    DelayTime = 6000;
            } else if (itemType == VAMPIRE_CORPSE) {
                VampireCorpse* pVampireCorpse = dynamic_cast<VampireCorpse*>(pItem);
                pVampireCorpse->setXY(pt.x, pt.y);

                GCAddVampireCorpse gcAddVampireCorpse;
                makeGCAddVampireCorpse(&gcAddVampireCorpse, pVampireCorpse);
                broadcastPacket(pt.x, pt.y, &gcAddVampireCorpse);

                // Corpses disappear faster in the master lair.
                if (isMasterLair())
                    DelayTime = DELAY_MASTER_LAIR_DECAY_CORPSE;
                else
                    DelayTime = 6000;
            } else if (itemType == OUSTERS_CORPSE) {
                OustersCorpse* pOustersCorpse = dynamic_cast<OustersCorpse*>(pItem);
                pOustersCorpse->setXY(pt.x, pt.y);

                GCAddOustersCorpse gcAddOustersCorpse;
                makeGCAddOustersCorpse(&gcAddOustersCorpse, pOustersCorpse);
                broadcastPacket(pt.x, pt.y, &gcAddOustersCorpse);

                // Corpses disappear faster in the master lair.
                if (isMasterLair())
                    DelayTime = DELAY_MASTER_LAIR_DECAY_CORPSE;
                else
                    DelayTime = 6000;
            } else if (itemType == NPC_CORPSE) {
                Assert(false);
            } else if (itemType == MONSTER_CORPSE) {
                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                GCAddMonsterCorpse gcAddMonsterCorpse;
                makeGCAddMonsterCorpse(&gcAddMonsterCorpse, pMonsterCorpse, pt.x, pt.y);
                broadcastPacket(pt.x, pt.y, &gcAddMonsterCorpse);

                isFlag = de::gameContext().flags().isFlagPole(pMonsterCorpse);

                // Corpses disappear faster in the master lair.
                if (isMasterLair()) {
                    MonsterType_t mt = pMonsterCorpse->getMonsterType();
                    const MonsterInfo* pMonsterInfo = de::gameContext().monsterInfos().getMonsterInfo(mt);
                    Assert(pMonsterInfo != NULL);

                    // A master's corpse uses its own decay delay.
                    if (pMonsterInfo->isMaster()) {
                        // Multiplied by 10 because it holds no items.
                        DelayTime = DELAY_MASTER_LAIR_DECAY_MASTER_CORPSE * 10;
                    } else {
                        DelayTime = DELAY_MASTER_LAIR_DECAY_CORPSE;
                    }
                } else
                    DelayTime = 600;

                sendRelicEffect(pMonsterCorpse, this, pt.x, pt.y);

                // This is temporary.
                // An item normally does not carry zone coordinates, but
                // the relic table needs them.
                pMonsterCorpse->setX(pt.x);
                pMonsterCorpse->setY(pt.y);
                pMonsterCorpse->setZone(this);

                isShrine = pMonsterCorpse->isShrine() && !de::gameContext().flags().isFlagPole(pMonsterCorpse);

                // A shrine is shown on the minimap.
                if (isShrine) {
                    NPCInfo* pNPCInfo = new NPCInfo();
                    pNPCInfo->setName(pMonsterCorpse->getName());
                    pNPCInfo->setNPCID(pMonsterCorpse->getMonsterType());
                    pNPCInfo->setX(pt.x);
                    pNPCInfo->setY(pt.y);

                    addNPCInfo(pNPCInfo);
                }
            } else {
                Assert(false);
            }

            // A corpse holding no items gets a shorter delay.
            Corpse* pCorpse = dynamic_cast<Corpse*>(pItem);
            if (pCorpse->getTreasureCount() == 0) {
                DelayTime = DelayTime / 10;
            }
            // A relic does not disappear as time passes.
            if (!isShrine && !isFlag && !pCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE) &&
                !pCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE) &&
                !pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_GUARD) &&
                !pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_HOLY) && pCorpse->getTreasureCount() < 200) {
                // Explicitly requested delay.
                if (decayTurn != 0)
                    DelayTime = decayTurn;

                // An item dropped on the ground disappears after a while.
                EffectDecayCorpse* pEffectDecayCorpse =
                    new EffectDecayCorpse(this, pt.x, pt.y, (Corpse*)pItem, DelayTime);
                m_ObjectRegistry.registerObject(pEffectDecayCorpse);
                addEffect(pEffectDecayCorpse);
            } else {
                // A flagpole must not block.
                if (!isFlag) {
                    // The relic table is an item (a corpse) but
                    // it has to block.
                    Tile& rTile = getTile(pt.x, pt.y);

                    rTile.setBlocked(Creature::MOVE_MODE_WALKING);
                    rTile.setBlocked(Creature::MOVE_MODE_BURROWING);

                    // Store the relic table's position.
                    m_RelicTableOID = pCorpse->getObjectID();
                    m_RelicTableX = pt.x;
                    m_RelicTableY = pt.y;
                }
            }
        } else {
            GCDropItemToZone gcDropItemToZone;
            makeGCDropItemToZone(&gcDropItemToZone, pItem, pt.x, pt.y);
            gcDropItemToZone.setDropPetOID(DropPetOID);

            broadcastPacket(pt.x, pt.y, &gcDropItemToZone);

            // A motorcycle does not disappear over time.
            if (IClass == Item::ITEM_CLASS_MOTORCYCLE) {
                // Clear the check in case this is a transport.
                MotorcycleBox* pMotorcycleBox = de::gameContext().parking().getMotorcycleBox(pItem->getItemID());

                if (pMotorcycleBox != NULL) {
                    Motorcycle* pMotorcycle = pMotorcycleBox->getMotorcycle();
                    Assert(pMotorcycle != NULL);

                    // Optimized item save.
                    char pField[80];
                    snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                             getZoneID(), (int)pt.x, (int)pt.y);

                    pMotorcycle->tinysave(pField);

                    pMotorcycleBox->setZone(this);
                    pMotorcycleBox->setX(pt.x);
                    pMotorcycleBox->setY(pt.y);

                    pMotorcycleBox->setTransport(false);
                }
            } else if (isRelicItem(IClass)) {
                // A relic does not disappear.
                addEffectRelicPosition(pItem, getZoneID(), pt);
                char pField[80];
                snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                         getZoneID(), pt.x, pt.y);
                pItem->tinysave(pField);
            } else {
                // Items are removed after 3 minutes instead of 10.
                Turn_t DelayTime = 1800;

                // Items disappear faster in the master lair.
                if (isMasterLair()) {
                    DelayTime = DELAY_MASTER_LAIR_DECAY_ITEM;
                }

                if (!pItem->isFlagItem() && IClass != Item::ITEM_CLASS_SWEEPER) {
                    // Explicitly requested delay.
                    if (decayTurn != 0)
                        DelayTime = decayTurn;

                    // An item dropped on the ground disappears after a while.
                    EffectDecayItem* pEffectDecayItem = new EffectDecayItem(this, pt.x, pt.y, (Item*)pItem, DelayTime);
                    pEffectDecayItem->setNextTime(999999);
                    m_ObjectRegistry.registerObject(pEffectDecayItem);
                    addEffect(pEffectDecayItem);
                } else {
                    char pField[80];
                    snprintf(pField, sizeof(pField), "OwnerID='', Storage=%d, StorageID=%u, X=%d, Y=%d", STORAGE_ZONE,
                             getZoneID(), pt.x, pt.y);
                    pItem->tinysave(pField);
                }
            }
        }

        return pt;
    } else {
        TPOINT pt_error;
        pt_error.x = -1;
        pt_error.y = -1;

        return pt_error;
    }

    __END_PROFILE_ZONE("Z_ADD_ITEM")
    return pt;

    __END_DEBUG
    __END_CATCH
}

//--------------------------------------------------------------------------------
// get Item
//--------------------------------------------------------------------------------
Item* Zone::getItem(ObjectID_t id) const

{
    unordered_map<ObjectID_t, Item*>::const_iterator iItem = m_Items.find(id);

    if (iItem != m_Items.end()) {
        return iItem->second;
    }

    return NULL;
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void Zone::deleteItem(Object* pObject, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    __BEGIN_PROFILE_ZONE("Z_DELETE_ITEM")

    deleteFromItemList(pObject->getObjectID());

    //--------------------------------------------------
    // Delete the object from the zone.
    //--------------------------------------------------
    getTile(x, y).deleteItem();


    if (pObject->getObjectClass() == Object::OBJECT_CLASS_ITEM) {
        // A relic table's block must be cleared.
        Item* pItem = dynamic_cast<Item*>(pObject);
        Assert(pItem != NULL);
        if (pItem->getItemClass() == Item::ITEM_CLASS_CORPSE && pItem->getItemType() == MONSTER_CORPSE) {
            MonsterCorpse* pCorpse = dynamic_cast<MonsterCorpse*>(pItem);
            Assert(pCorpse != NULL);

            if (pCorpse->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE) ||
                pCorpse->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE) ||
                pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_GUARD) ||
                pCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_HOLY)) {
                Tile& rTile = getTile(x, y);

                // Clear the block.
                rTile.clearBlocked(Creature::MOVE_MODE_WALKING);
                rTile.clearBlocked(Creature::MOVE_MODE_BURROWING);
            }
        }

        if (isRelicItem(pItem)) {
            deleteEffectRelicPosition(pItem);
        }
    }

    //--------------------------------------------------
    // Broadcast to nearby PCs that the object is gone.
    //--------------------------------------------------


    __END_PROFILE_ZONE("Z_DELETE_ITEM")

    __END_CATCH
}

void Zone::addToItemList(Item* pItem) {
    __BEGIN_TRY

    m_Items[pItem->getObjectID()] = pItem;

    __END_CATCH
}

void Zone::deleteFromItemList(ObjectID_t id) {
    __BEGIN_TRY

    unordered_map<ObjectID_t, Item*>::iterator itr = m_Items.find(id);

    if (itr == m_Items.end()) {
        return;
    }

    m_Items.erase(itr);

    __END_CATCH
}

void Zone::addVampirePortal(ZoneCoord_t cx, ZoneCoord_t cy, Vampire* pVampire, const ZONE_COORD& ZoneCoord)

{
    __BEGIN_TRY

    Assert(m_OuterRect.ptInRect(cx, cy));
    Assert(pVampire != NULL);

    // Compute how many can enter and how long it lasts from the vampire's stats.
    // It is not added to the zone immediately, so a small delay is added.
    Duration_t duration = (60 + (pVampire->getINT(ATTR_CURRENT) - 20) / 3) * 10 + 20; // the unit is 0.1 second
    int count = 3 + (pVampire->getINT(ATTR_CURRENT) - 20) / 10;

    // Create the effect object itself.
    EffectVampirePortal* pEffectVampirePortal = new EffectVampirePortal(this, cx, cy);
    pEffectVampirePortal->setDeadline(duration);
    pEffectVampirePortal->setOwnerID(pVampire->getName());
    pEffectVampirePortal->setZoneCoord(ZoneCoord.id, ZoneCoord.x, ZoneCoord.y);
    pEffectVampirePortal->setCount(count);

    // Create and add the effect schedule.
    EffectSchedule* pEffectSchedule = new EffectSchedule;
    pEffectSchedule->setEffect(pEffectVampirePortal);
    pEffectSchedule->addWork(WORKCODE_ADD_VAMPIRE_PORTAL, NULL);
    m_pEffectScheduleManager->addEffectSchedule(pEffectSchedule);

    __END_CATCH
}

//-------------------------------------------------------------
// deleteMotorcycle( x, y, pMotorcycle )
//-------------------------------------------------------------
// Instead of deleting it right away, an EffectDecayItem is attached so that
// the zone's heartbeat deletes it.
//-------------------------------------------------------------
void Zone::deleteMotorcycle(ZoneCoord_t cx, ZoneCoord_t cy, Motorcycle* pMotorcycle)

{
    __BEGIN_TRY

    Assert(m_OuterRect.ptInRect(cx, cy));
    Assert(pMotorcycle != NULL);

    EffectDecayItem* pEffectDecayItem = new EffectDecayItem(this, cx, cy, (Item*)pMotorcycle, 0,
                                                            false); // Not deleted from the DB.
    pEffectDecayItem->setNextTime(999999);
    m_ObjectRegistry.registerObject(pEffectDecayItem);
    addEffect_LOCKING(pEffectDecayItem);

    __END_CATCH
}

//-------------------------------------------------------------
// transportItemToCorpse
//-------------------------------------------------------------
// Move pItem from this zone to (cx, cy) of pZone.
// The move is done by attaching an EffectTransportItem.
//-------------------------------------------------------------
void Zone::transportItemToCorpse(Item* pItem, Zone* pTargetZone, ObjectID_t corpseObjectID)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    if (pTargetZone->getZoneGroup() == this->getZoneGroup()) {
        //  Same zone group, so move it right away.

        Item* pCorpseItem = pTargetZone->getItem(corpseObjectID);

        if (pCorpseItem == NULL) {
            StringStream msg;
            msg << "[" << (int)m_ZoneID << "] no corpse: corpseObjectID=" << (int)corpseObjectID;

            throw Error(msg.toString());
        } else if (pCorpseItem->getItemClass() != Item::ITEM_CLASS_CORPSE) {
            StringStream msg;
            msg << "[" << (int)m_ZoneID << "] not a corpse: corpseObjectID=" << (int)corpseObjectID
                << ", itemClass=" << (int)pCorpseItem->getItemClass()
                << ", itemType=" << (int)pCorpseItem->getItemType();

            throw Error(msg.toString());
        } else {
            Corpse* pCorpse = dynamic_cast<Corpse*>(pCorpseItem);
            Assert(pCorpse != NULL);

            pCorpse->addTreasure(pItem);
        }
    } else {
        EffectTransportItemToCorpse* pEffectTransportItem =
            new EffectTransportItemToCorpse(this, pItem, pTargetZone, corpseObjectID, 0);
        pEffectTransportItem->setNextTime(999999);
        m_ObjectRegistry.registerObject(pEffectTransportItem);
        addEffect_LOCKING(pEffectTransportItem);
    }

    __END_CATCH
}

//-------------------------------------------------------------
// transportItem
//-------------------------------------------------------------
// Move pItem from this zone to (cx, cy) of pZone.
// The move is done by attaching an EffectTransportItem.
//-------------------------------------------------------------
void Zone::transportItem(ZoneCoord_t x, ZoneCoord_t y, Item* pItem, Zone* pZone, ZoneCoord_t cx, ZoneCoord_t cy)

{
    __BEGIN_TRY


    Assert(m_OuterRect.ptInRect(x, y));
    Assert(pItem != NULL);

    if (pZone->getZoneGroup() == this->getZoneGroup()) {
        //  Same zone group, so move it right away.
        deleteFromItemList(pItem->getObjectID());
        getTile(x, y).deleteItem();

        // Send the packet saying the item is gone.
        GCDeleteObject gcDeleteObject;
        gcDeleteObject.setObjectID(pItem->getObjectID());

        broadcastPacket(x, y, &gcDeleteObject);

        pZone->getObjectRegistry().registerObject(pItem);
        pZone->addItem(pItem, cx, cy);
    } else {
        EffectTransportItem* pEffectTransportItem = new EffectTransportItem(this, x, y, pZone, cx, cy, pItem, 0);
        pEffectTransportItem->setNextTime(999999);
        m_ObjectRegistry.registerObject(pEffectTransportItem);
        addEffect_LOCKING(pEffectTransportItem);
    }

    __END_CATCH
}

//-------------------------------------------------------------
// add Item To Corpse Delayed
//-------------------------------------------------------------
// Adds an item; may be called from another thread.
// The actual add happens in a later heartbeat.
//-------------------------------------------------------------
void Zone::addItemToCorpseDelayed(Item* pItem, ObjectID_t corpseItemID)

{
    __BEGIN_TRY

    Assert(pItem != NULL);

    EffectAddItemToCorpse* pEffectAddItem = new EffectAddItemToCorpse(this, pItem, corpseItemID, 0);
    pEffectAddItem->setNextTime(999999);
    m_ObjectRegistry.registerObject(pEffectAddItem);
    addEffect_LOCKING(pEffectAddItem);

    __END_CATCH
}

//-------------------------------------------------------------
// add Item Delayed
//-------------------------------------------------------------
// Adds an item; may be called from another thread.
// The actual add happens in a later heartbeat.
//-------------------------------------------------------------
void Zone::addItemDelayed(Item* pItem, ZoneCoord_t cx, ZoneCoord_t cy, bool bAllowCreature)

{
    __BEGIN_TRY

    Assert(m_OuterRect.ptInRect(cx, cy));
    Assert(pItem != NULL);

    EffectAddItem* pEffectAddItem = new EffectAddItem(this, cx, cy, pItem, 0, bAllowCreature);
    pEffectAddItem->setNextTime(999999);
    m_ObjectRegistry.registerObject(pEffectAddItem);
    addEffect_LOCKING(pEffectAddItem);

    __END_CATCH
}

// This code has not been tested yet.
void Zone::deleteItemDelayed(Object* pObject, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    Assert(m_OuterRect.ptInRect(x, y));
    Assert(pObject != NULL);

    EffectDeleteItem* pEffectDeleteItem = new EffectDeleteItem(this, x, y, pObject, 0);
    pEffectDeleteItem->setNextTime(999999);
    m_ObjectRegistry.registerObject(pEffectDeleteItem);
    addEffect_LOCKING(pEffectDeleteItem);

    __END_CATCH
}

//-------------------------------------------------------------
// add Relic Item
//-------------------------------------------------------------
// Adds an item; may be called from another thread.
// The actual add happens in a later heartbeat.
//-------------------------------------------------------------
bool Zone::addRelicItem(int relicIndex)

{
    __BEGIN_TRY


    const RelicInfo* pRelicInfo =
        dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, relicIndex));

    int cx = pRelicInfo->x;
    int cy = pRelicInfo->y;

    Assert(m_OuterRect.ptInRect(cx, cy));

    // A relic table already exists.
    if (m_bHasRelicTable) {
        return false;
        // Find this zone's relic table. (remember x, y in the Zone when addItem runs)
        // Return if the relic table holds no relic.
        // Otherwise put a relic that is not its own back into the original relic table.
        // addItemDelayed can add it to the original zone.
    } else {
        // Create the monster.
        Monster* pMonster = NULL;
        try {
            pMonster = new Monster(pRelicInfo->monsterType);

            m_ObjectRegistry.registerObject(pMonster);

        } catch (Throwable&) {
            SAFE_DELETE(pMonster);
            return false;
        }


        // Create the MonsterCorpse (the relic table).
        MonsterCorpse* pMonsterCorpse = NULL;
        try {
            pMonsterCorpse = new MonsterCorpse(pMonster);
            pMonsterCorpse->setDir(2);
            pMonsterCorpse->setZone(this);
            pMonsterCorpse->setX(cx);
            pMonsterCorpse->setY(cy);
            Assert(pMonsterCorpse != NULL);
        } catch (Throwable& t) {
        }


        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER) {
            Effect* pRelicTable = new EffectSlayerRelicTable(pMonsterCorpse);
            pRelicTable->setNextTime(999999);
            m_ObjectRegistry.registerObject(pRelicTable);

            pMonsterCorpse->getEffectManager().addEffect(pRelicTable);
            pMonsterCorpse->setFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE);

            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_SLAYER);
        } else {
            Effect* pRelicTable = new EffectVampireRelicTable(pMonsterCorpse);
            pRelicTable->setNextTime(999999);
            m_ObjectRegistry.registerObject(pRelicTable);

            pMonsterCorpse->getEffectManager().addEffect(pRelicTable);
            pMonsterCorpse->setFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE);

            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_VAMPIRE);
        }

        // Create the relic.
        list<OptionType_t> optionNULL;
        Item* pItem = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_RELIC, relicIndex, optionNULL);
        Assert(pItem != NULL);


        // Mark this zone as holding a relic table.
        m_bHasRelicTable = true;

        pMonsterCorpse->addTreasure(pItem);

        // The relic is created in the DB here.
        // CGDissectionCorpseHandler therefore does not create it,
        // so it is not created again every time it is taken out of the table.
        pItem->create("", STORAGE_CORPSE, pMonsterCorpse->getObjectID(), 0, 0);

        if (pRelicInfo->relicType == RELIC_TYPE_SLAYER) {
            EffectSlayerRelic* pEffect = new EffectSlayerRelic(pMonsterCorpse);
            pMonsterCorpse->getEffectManager().addEffect(pEffect);
            pMonsterCorpse->setFlag(Effect::EFFECT_CLASS_SLAYER_RELIC);
            pEffect->affect(pMonsterCorpse);
            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_SLAYER);
        } else {
            EffectVampireRelic* pEffect = new EffectVampireRelic(pMonsterCorpse);
            pMonsterCorpse->getEffectManager().addEffect(pEffect);
            pMonsterCorpse->setFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC);
            pEffect->affect(pMonsterCorpse);
            de::gameContext().combatInfo().setRelicOwner(relicIndex, CombatInfoManager::RELIC_OWNER_VAMPIRE);
        }


        // It must not be added to the zone directly (synchronization), so the add
        // goes through an effect.
        EffectAddItem* pEffectAddItem = new EffectAddItem(this, cx, cy, pMonsterCorpse, 0, false);
        pEffectAddItem->setNextTime(999999);
        m_ObjectRegistry.registerObject(pEffectAddItem);

        addEffect_LOCKING(pEffectAddItem);
    }

    return true;

    __END_CATCH
}

//-------------------------------------------------------------
// delete Relic Item
//-------------------------------------------------------------
// Deletes an item; may be called from another thread.
// The actual delete happens in a later heartbeat.
//-------------------------------------------------------------
bool Zone::deleteRelicItem()

{
    __BEGIN_TRY

    // Return if there is no relic table.
    if (!m_bHasRelicTable) {
        return false;
    }

    // Find the relic table.
    Item* pItem = dynamic_cast<Item*>(getTile(m_RelicTableX, m_RelicTableY).getObject(m_RelicTableOID));
    Assert(pItem != NULL);

    // It must not be removed from the zone directly (synchronization), so the
    // delete goes through an effect.
    EffectDeleteItem* pEffectDeleteItem = new EffectDeleteItem(this, m_RelicTableX, m_RelicTableY, pItem, 0);
    pEffectDeleteItem->setNextTime(999999);
    m_ObjectRegistry.registerObject(pEffectDeleteItem);

    addEffect_LOCKING(pEffectDeleteItem);


    m_bHasRelicTable = false;

    return true;

    __END_CATCH
}
