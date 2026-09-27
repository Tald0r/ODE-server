//////////////////////////////////////////////////////////////////////////////
// Filename    : CGDissectionCorpseHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGDissectionCorpse.h"

#ifdef __GAME_SERVER__
#include <stdio.h>

#include "AlignmentManager.h"
#include "BloodBible.h"
#include "CastleInfoManager.h"
#include "CastleShrineInfoManager.h"
#include "CombatInfoManager.h"
#include "Corpse.h"
#include "CreatureUtil.h"
#include "DatabaseError.h"
#include "Effect.h"
#include "EffectFlagInsert.h"
#include "EffectHasSlayerRelic.h"
#include "EffectHasVampireRelic.h"
#include "EffectPrecedence.h"
#include "EffectRelicTable.h"
#include "GCAddEffect.h"
#include "GCCreateItem.h"
#include "GCRemoveCorpseHead.h"
#include "GCRemoveEffect.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "LevelWarManager.h"
#include "MonsterCorpse.h"
#include "MonsterInfo.h"
#include "Ousters.h"
#include "OustersCorpse.h"
#include "PCOustersInfo3.h"
#include "PCSlayerInfo3.h"
#include "PCVampireInfo3.h"
#include "PacketUtil.h"
#include "Party.h"
#include "PetInfo.h"
#include "PetUtil.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "Slayer.h"
#include "SlayerCorpse.h"
#include "StringPool.h"
#include "UniqueItemManager.h"
#include "Vampire.h"
#include "VampireCorpse.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "ZoneUtil.h"
#include "ctf/FlagManager.h"
#include "repository/ItemRepository.h"
#include "war/WarSystem.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGDissectionCorpseHandler::execute(CGDissectionCorpse* pPacket, Player* pPlayer)

{
    __BEGIN_TRY __BEGIN_DEBUG_EX

        StringPool& strings = de::gameContext().strings();

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        Creature* pCreature = pGamePlayer->getCreature();
        Zone* pZone = pCreature->getZone();
        ZoneCoord_t ZoneX = pPacket->getX();
        ZoneCoord_t ZoneY = pPacket->getY();

        // Check that the bounds are not crossed.
        if (!isValidZoneCoord(pZone, ZoneX, ZoneY)) {
            return;
        }

        Tile& rTile = pZone->getTile(ZoneX, ZoneY);
        Item* pItem = rTile.getItem();

        // No looting in the Coma state.
        // Return while currently a bat
        if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
            (pCreature->isVampire() && pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)))
            return;

        // A Slayer riding a motorcycle cannot loot items
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            if (pSlayer->hasRideMotorcycle()) {
                return;
            }
        }

        // An Ousters riding a sylph cannot loot
        if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
                return;
            }
        }

        // Return if there is no item on the ground
        // Return if the object id does not match
        // Return if the item on the ground is not a corpse
        if (pItem == NULL || pItem->getObjectID() != pPacket->getObjectID() ||
            pItem->getItemClass() != Item::ITEM_CLASS_CORPSE) {
            return;
        }

        bool bSlayerRelicTable = pItem->isFlag(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE);
        bool bVampireRelicTable = pItem->isFlag(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE);

        // A Slayer cannot click a Slayer relic table, and
        // a Vampire cannot click a Vampire relic table either.
        if ((pCreature->isSlayer() && bSlayerRelicTable) || (pCreature->isVampire() && bVampireRelicTable)) {
            return;
        }

        // A shrine cannot be clicked outside a war.
        // A guardian shrine cannot be clicked by the defending side.
        if (pItem->getItemType() == MONSTER_CORPSE) {
            MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
            if (pMonsterCorpse->getTreasureCount() > 200) {
                // Quest corpse. Surely no monster really drops 200 loot items.
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                pPC->getGQuestManager()->touchWayPoint(pMonsterCorpse);
                return;
            }

            if (de::gameContext().flags().isFlagPole(pMonsterCorpse)) {
                if (!de::gameContext().flags().hasFlagWar())
                    return;
            } else if (pZone->getLevelWarManager() != NULL && pZone->getLevelWarManager()->isSafe(pMonsterCorpse)) {
                if (!pZone->getLevelWarManager()->hasWar())
                    return;
                else if ((pCreature->isSlayer() && pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 0) ||
                         (pCreature->isVampire() && pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 1) ||
                         (pCreature->isOusters() && pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 2)) {
                    return;
                }
            } else if (pMonsterCorpse->isShrine()) {
                // Nothing can be taken out while the shield is up.
                if (pMonsterCorpse->isFlag(Effect::EFFECT_CLASS_SHRINE_SHIELD))
                    return;

                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC);

                ZoneID_t castleZoneID;
                bool isCastle;

                bool hasWar = false;

                isCastle = de::gameContext().castleInfos().getCastleZoneID(pZone->getZoneID(), castleZoneID);

                if (isCastle && de::gameContext().warSystem().hasCastleActiveWar(castleZoneID)) {
                    // Guild war handling

                    // In a castle the defending side cannot click.
                    CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(castleZoneID);
                    if (pCastleInfo->getRace() != pPC->getRace())
                        return;

                    hasWar = true;
                }

                if (de::gameContext().warSystem().hasActiveRaceWar()) {
                    // Race war handling

                    // In a castle the defending side cannot click.
                    if (de::gameContext().shrines().isDefenderOfGuardShrine(pPC, pMonsterCorpse))
                        return;

                    hasWar = true;
                }

                // With no war at all, nothing can be taken out of the shrine.
                if (!hasWar) {
                    return;
                }
            }
        }

        // Reaching here means the other race's relic table was clicked.
        // In that case, how much of the EffectRelic is left has to be checked.
        EffectRelicTable* pRelicTableEffect = NULL;

        // Get the Effect attached, according to the relic table kind.
        if (bSlayerRelicTable) {
            pRelicTableEffect = dynamic_cast<EffectRelicTable*>(
                pItem->getEffectManager().findEffect(Effect::EFFECT_CLASS_SLAYER_RELIC_TABLE));
        } else if (bVampireRelicTable) {
            pRelicTableEffect = dynamic_cast<EffectRelicTable*>(
                pItem->getEffectManager().findEffect(Effect::EFFECT_CLASS_VAMPIRE_RELIC_TABLE));
        }

        // Is it time to take an item out of the relic table?
        // Not before the SafeTime.
        // Not during the LockTime.
        if (pRelicTableEffect != NULL && (!pRelicTableEffect->isSafeTime() || pRelicTableEffect->isLockTime())) {
            GCSystemMessage gcSystemMessage;
            gcSystemMessage.setMessage(strings.getString(STRID_CANNOT_TAKE_RELIC_NOW));

            pGamePlayer->sendPacket(&gcSystemMessage);
            return;
        }

        // The alignment has to be checked.
        if (pItem->getItemType() != MONSTER_CORPSE) {
            if (pPacket->isPet())
                return;
            if (!pZone->isPKZone()) {
                if (pItem->getItemType() == SLAYER_CORPSE && pCreature->isSlayer()) {
                    SlayerCorpse* pSlayerCorpse = dynamic_cast<SlayerCorpse*>(pItem);

                    PCSlayerInfo3& rPCSlayerInfo = pSlayerCorpse->getSlayerInfo();

                    // If it is not one's own corpse
                    if (rPCSlayerInfo.getName() != pCreature->getName()) {
                        // An evil one's corpse gets no protection....
                        if (de::gameContext().alignments().getAlignmentType(rPCSlayerInfo.getAlignment()) >= NEUTRAL) {
                            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                            // The alignment drops by 500
                            Alignment_t NewAlignment = max(-10000, pSlayer->getAlignment() - 500);

                            pSlayer->setAlignment(NewAlignment);
                            GCModifyInformation gcModifyInformation;
                            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
                            pGamePlayer->sendPacket(&gcModifyInformation);
                        }
                    }
                } else if (pItem->getItemType() == VAMPIRE_CORPSE && pCreature->isVampire()) {
                    PCVampireInfo3& rPCVampireInfo = dynamic_cast<VampireCorpse*>(pItem)->getVampireInfo();

                    // If it is not one's own corpse
                    if (rPCVampireInfo.getName() != pCreature->getName()) {
                        // An evil one's corpse gets no protection....
                        if (de::gameContext().alignments().getAlignmentType(rPCVampireInfo.getAlignment()) >= NEUTRAL) {
                            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                            // The alignment drops by 500
                            Alignment_t NewAlignment = max(-10000, pVampire->getAlignment() - 500);

                            pVampire->setAlignment(NewAlignment);
                            GCModifyInformation gcModifyInformation;
                            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
                            pGamePlayer->sendPacket(&gcModifyInformation);
                        }
                    }
                } else if (pItem->getItemType() == OUSTERS_CORPSE && pCreature->isOusters()) {
                    PCOustersInfo3& rPCOustersInfo = dynamic_cast<OustersCorpse*>(pItem)->getOustersInfo();

                    // If it is not one's own corpse
                    if (rPCOustersInfo.getName() != pCreature->getName()) {
                        // An evil one's corpse gets no protection....
                        if (de::gameContext().alignments().getAlignmentType(rPCOustersInfo.getAlignment()) >= NEUTRAL) {
                            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                            // The alignment drops by 500
                            Alignment_t NewAlignment = max(-10000, pOusters->getAlignment() - 500);

                            pOusters->setAlignment(NewAlignment);
                            GCModifyInformation gcModifyInformation;
                            gcModifyInformation.addLongData(MODIFY_ALIGNMENT, NewAlignment);
                            pGamePlayer->sendPacket(&gcModifyInformation);
                        }
                    }
                }
            }
        } else {
            MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            Assert(pPC != NULL);

            if (pPacket->isPet()) {
                if (pPC->getPetInfo() == NULL)
                    return; //(|| pMonsterCorpse->isNextTreasureHead() ) return;
            }

            if (pMonsterCorpse != NULL && de::gameContext().flags().isFlagPole(pMonsterCorpse)) {
                if (!de::gameContext().flags().hasFlagWar())
                    return;
                // The same race cannot pull it out..
                if (de::gameContext().flags().getFlagPoleRace(pMonsterCorpse) == pPC->getRace())
                    return;

                TPOINT tp;
                if (pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG))
                    return;
                if (!pPC->getInventory()->getEmptySlot(1, 2, tp))
                    return;

                if (pPC->isFlag(Effect::EFFECT_CLASS_CASKET) || pPC->isDead() ||
                    pPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) || pPC->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD))
                    return;

                if (pPC->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
                    if (pSlayer->hasRideMotorcycle())
                        return;
                }

                if (pPC->isFlag(Effect::EFFECT_CLASS_HIDE) || pPC->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_FADE_OUT) || pPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
                    pPC->isFlag(Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL))
                    return;
            }
        }

        Corpse* pCorpse = dynamic_cast<Corpse*>(pItem);
        bool bDissectAll = false;

        // With more than 3 items in the corpse, everything comes out at once.
        // Items a pet pulls out all come out at once.
        // 2003.1.14  by bezz, Sequoia, sigi
        if (pCorpse->getTreasureCount() > 3 || pPacket->isPet())
            bDissectAll = true;

        int treasureCount = 0;
        ObjectID_t PetOID = 0;

        if (pPacket->isPet())
            PetOID = pCreature->getObjectID();

        do // while (bDissectAll);
        {
            if (pPacket->isPet() && pCorpse->getItemType() == MONSTER_CORPSE) {
                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                PetInfo* pPetInfo = pPC->getPetInfo();

                if (pPetInfo == NULL)
                    break;
                if (pMonsterCorpse->isNextTreasureHead() && pPetInfo->canCutHead() == 0)
                    break;
            }

            Item* pTreasure = pCorpse->getTreasure();

            if (pTreasure == NULL)
                break;

            if (pPacket->isPet())
                pTreasure->setFlag(Effect::EFFECT_CLASS_PET_DISSECT);

            ////////////////////////////////////////////////////////////////
            // 2002 Children's Day event
            // The black star item is exchanged for a family photo voucher.
            // So it cannot come out beyond a fixed count
            // (7 in Transylvania, 3 in Wallachia).
            // When a black star item does come out,
            // count the items and keep it from appearing past the limit.

            if (pTreasure->getItemClass() == Item::ITEM_CLASS_EVENT_STAR && pTreasure->getItemType() == 0) {
                int BlackStarNumber = 0;

                try {
                    if (!defaultItemRepository().loadBlackStarCount(BlackStarNumber)) {
                        throw ProtocolException("Fail to load data from DB");
                    }
                } catch (const DatabaseError& error) {
                    // A SQL failure arrives as END_DB's DatabaseError, which
                    // the handler's outer catch (Throwable&) would not
                    // swallow, so it is rethrown as an Error carrying the line
                    // END_DB wrote to DBError.log.
                    throw Error("CGDissectionCorpseHandler: the black-star count read failed : " + error.message());
                }

                {
                    if (BlackStarNumber >= 7)
                        cout << "Black star count reached the limit" << endl;
                    else {
                        (pZone->getObjectRegistry()).registerObject(pTreasure);

                        TPOINT pt = pZone->addItem(pTreasure, ZoneX, ZoneY);
                        if (pt.x != -1) {
                            if (pItem->getItemType() == MONSTER_CORPSE) {
                                MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);

                                // An item out of a monster corpse must get the precedence effect.
                                // Delete any effect that is already there and
                                // add the effect anew.
                                const string& HostName = pMonsterCorpse->getHostName();
                                int HostPartyID = pMonsterCorpse->getHostPartyID();

                                bool isPrecedence = true;
                                if (pTreasure->getItemClass() == Item::ITEM_CLASS_EVENT_ITEM &&
                                    pTreasure->getItemType() == 3) {
                                    isPrecedence = false;
                                }

                                // by sigi. 2002.12.12
                                if (isPrecedence && (!HostName.empty() || HostPartyID != 0)) {
                                    EffectPrecedence* pEffectPrecedence = new EffectPrecedence(pTreasure);
                                    pEffectPrecedence->setDeadline(100);
                                    pEffectPrecedence->setHostName(HostName);
                                    pEffectPrecedence->setHostPartyID(HostPartyID);
                                    EffectManager& rEffectManager = pTreasure->getEffectManager();
                                    rEffectManager.deleteEffect(Effect::EFFECT_CLASS_PRECEDENCE);
                                    rEffectManager.addEffect(pEffectPrecedence);
                                    pTreasure->setFlag(Effect::EFFECT_CLASS_PRECEDENCE);
                                }

                                // For a skull, a packet telling the others to remove the head from the corpse must be sent.
                                if (pTreasure->getItemClass() == Item::ITEM_CLASS_SKULL) {
                                    // Cut the head off
                                    pMonsterCorpse->removeHead();

                                    GCRemoveCorpseHead _GCRemoveCorpseHead;
                                    _GCRemoveCorpseHead.setObjectID(pItem->getObjectID());
                                    pZone->broadcastPacket(ZoneX, ZoneY,
                                                           &_GCRemoveCorpseHead); // the original corpse coordinates

                                    if (pCreature->getPartyID() != 0 && HostPartyID == pCreature->getPartyID()) {
                                        Party* pParty =
                                            pCreature->getLocalPartyManager()->getParty(pCreature->getPartyID());
                                        if (pParty != NULL) {
                                            pParty->dissectCorpse(pCreature, pMonsterCorpse);
                                        }
                                    }
                                }
                            }

                            // Keep the existing ItemID.
                            // An ItemID of 0 means create() hands out a new ItemID.
                            // by sigi. 2002.10.28
                            pTreasure->create("", STORAGE_ZONE, pZone->getZoneID(), pt.x, pt.y, pTreasure->getItemID());
                        } else {
                            SAFE_DELETE(pTreasure);
                        }
                    }
                }
            } else if (pTreasure->isFlagItem()) {
                // A flag goes straight into the inventory.
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                Assert(pPC != NULL);

                Inventory* pInventory = pPC->getInventory();
                Assert(pInventory != NULL);

                TPOINT tp;

                if (!pInventory->getEmptySlot(pTreasure, tp) || pCorpse->getItemType() != MONSTER_CORPSE ||
                    !de::gameContext().flags().getFlag(pPC, dynamic_cast<MonsterCorpse*>(pCorpse))) {
                    // Put it back in.
                    pCorpse->addTreasure(pTreasure);

                    pCorpse->setFlag(Effect::EFFECT_CLASS_FLAG_INSERT);
                    EffectFlagInsert* pFlagEffect = new EffectFlagInsert(pCorpse);
                    pCorpse->getEffectManager().addEffect(pFlagEffect);

                    GCAddEffect gcAddEffect;
                    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_FLAG_INSERT);
                    gcAddEffect.setObjectID(pCorpse->getObjectID());
                    gcAddEffect.setDuration(65535);

                    pCorpse->getZone()->broadcastPacket(pCorpse->getX(), pCorpse->getY(), &gcAddEffect);

                    continue;
                }

                pInventory->addItem(pTreasure, tp);

                char pField[80];
                snprintf(pField, sizeof(pField), "OwnerID='%s', Storage=%d, StorageID=0, X=%d, Y=%d",
                         pPC->getName().c_str(), STORAGE_INVENTORY, tp.x, tp.y);
                pTreasure->tinysave(pField);
                addSimpleCreatureEffect(pPC, Effect::EFFECT_CLASS_HAS_FLAG);

                GCCreateItem gcCreateItem;

                makeGCCreateItem(&gcCreateItem, pTreasure, tp.x, tp.y);
                pPC->getPlayer()->sendPacket(&gcCreateItem);
            } else {
                (pZone->getObjectRegistry()).registerObject(pTreasure);

                TPOINT pt = pZone->addItem(pTreasure, ZoneX, ZoneY, true, 0, PetOID);
                if (pt.x != -1) {
                    if (pItem->getItemType() == MONSTER_CORPSE) {
                        MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);

                        if (dissectionRelicItem(pCorpse, pTreasure, pt)) {
                            // When pTreasure is a relicItem
                        }

                        treasureCount++;

                        if (pZone->getLevelWarManager() != NULL &&
                            pZone->getLevelWarManager()->isSafe(pMonsterCorpse)) {
                            Effect* pEffect =
                                pMonsterCorpse->getEffectManager().findEffect(Effect::EFFECT_CLASS_KEEP_SWEEPER);
                            if (pEffect != NULL) {
                                pEffect->setDeadline(0);

                                GCRemoveEffect gcRE;
                                gcRE.setObjectID(pMonsterCorpse->getObjectID());
                                gcRE.addEffectList(pEffect->getSendEffectClass());

                                pZone->broadcastPacket(pMonsterCorpse->getX(), pMonsterCorpse->getY(), &gcRE);
                            }

                            // Broadcast to the zone once it is pulled out.
                            char safeRace[15];
                            if (pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 0) {
                                snprintf(safeRace, sizeof(safeRace), strings.c_str(STRID_SLAYER));
                            } else if (pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 1) {
                                snprintf(safeRace, sizeof(safeRace), strings.c_str(STRID_VAMPIRE));
                            } else if (pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 2) {
                                snprintf(safeRace, sizeof(safeRace), strings.c_str(STRID_OUSTERS));
                            } else if (pZone->getLevelWarManager()->getSafeIndex(pMonsterCorpse) == 3) {
                                snprintf(safeRace, sizeof(safeRace), strings.c_str(STRID_CENTER));
                            } else {
                                Assert(false);
                            }

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

                            const SweeperInfo* pSweeperInfo =
                                dynamic_cast<SweeperInfo*>(de::gameContext().itemInfos().getItemInfo(
                                    Item::ITEM_CLASS_SWEEPER, pTreasure->getItemType()));

                            char msg[100];
                            snprintf(msg, sizeof(msg), strings.c_str(STRID_PULL_OUT_SWEEPER), safeRace,
                                     pSweeperInfo->getName().c_str(), pCreature->getName().c_str(), race);
                            GCSystemMessage gcSystemMessage;
                            gcSystemMessage.setMessage(msg);
                            pZone->broadcastPacket(&gcSystemMessage);
                        }

                        // An item out of a monster corpse must get the precedence effect.
                        // Delete any effect that is already there and
                        // add the effect anew.
                        // Leave an existing effect alone. When a corpse is added to the ground
                        // and an item already on the ground goes in, no precedence effect is attached.
                        if (!bSlayerRelicTable && !bVampireRelicTable) {
                            const string& HostName = pMonsterCorpse->getHostName();
                            int HostPartyID = pMonsterCorpse->getHostPartyID();

                            // by sigi. 2002.12.12
                            if ((!HostName.empty() || HostPartyID != 0)
                                // by Sequoia. 2003. 2.28.
                                && !pTreasure->isFlag(Effect::EFFECT_CLASS_PRECEDENCE)) {
                                EffectPrecedence* pEffectPrecedence = new EffectPrecedence(pTreasure);
                                if (!pTreasure->isQuestItem()) {
                                    // A quest item cannot be picked up even by party members.
                                    pEffectPrecedence->setDeadline(100);
                                    pEffectPrecedence->setHostPartyID(HostPartyID);
                                    pEffectPrecedence->setHostName(HostName);
                                } else {
                                    // A quest item keeps its precedence until it disappears.
                                    pEffectPrecedence->setDeadline(999999);
                                    pEffectPrecedence->setHostName(pMonsterCorpse->getQuestHostName());
                                }
                                EffectManager& rEffectManager = pTreasure->getEffectManager();
                                rEffectManager.deleteEffect(Effect::EFFECT_CLASS_PRECEDENCE);
                                rEffectManager.addEffect(pEffectPrecedence);
                                pTreasure->setFlag(Effect::EFFECT_CLASS_PRECEDENCE);
                            }
                        }

                        // For a skull, a packet telling the others to remove the head from the corpse must be sent.
                        if (pTreasure->getItemClass() == Item::ITEM_CLASS_SKULL) {
                            // Cut the head off
                            pMonsterCorpse->removeHead();

                            GCRemoveCorpseHead _GCRemoveCorpseHead;
                            _GCRemoveCorpseHead.setObjectID(pItem->getObjectID());
                            pZone->broadcastPacket(ZoneX, ZoneY,
                                                   &_GCRemoveCorpseHead); // the original corpse coordinates

                            if (pCreature->getPartyID() != 0 &&
                                pMonsterCorpse->getHostPartyID() == pCreature->getPartyID()) {
                                Party* pParty = pCreature->getLocalPartyManager()->getParty(pCreature->getPartyID());
                                if (pParty != NULL) {
                                    pParty->dissectCorpse(pCreature, pMonsterCorpse);
                                }
                            }
                        }
                    }

                    // Save to the DB.
                    saveDissectionItem(pCreature, pTreasure, pt.x, pt.y);
                } else {
                    SAFE_DELETE(pTreasure);
                }
            }
        } while (bDissectAll);

        if (pItem->getItemType() == MONSTER_CORPSE) {
            MonsterCorpse* pMonsterCorpse = dynamic_cast<MonsterCorpse*>(pItem);
            // Give the pet experience
            if (pPacket->isPet() && treasureCount != 0) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                GCModifyInformation gcMI;
                PetExp_t exp = computePetExp(
                    getPCLevel(pPC),
                    de::gameContext().monsterInfos().getMonsterInfo(pMonsterCorpse->getMonsterType())->getLevel(),
                    pPC->getPetInfo(), pGamePlayer);
                if (!increasePetExp(pPC->getPetInfo(), exp, &gcMI)) {
                    pGamePlayer->sendPacket(&gcMI);
                } else {
                    sendPetInfo(pGamePlayer, true);
                }

                addOlympicStat(pPC, 8, exp);
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX __END_CATCH
}
