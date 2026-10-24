//////////////////////////////////////////////////////////////////////////////
// FileName 	: ZoneSpawn.cpp
// Description	: Zone spawn and despawn: putting creatures and objects into the zone and taking them out.
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

//////////////////////////////////////////////////////////////////////////////
// Comparison class used with the STL find_if algorithm.
//////////////////////////////////////////////////////////////////////////////
class isSameCreature {
public:
    isSameCreature(Creature* pCreature) : m_Creature(pCreature) {}

    bool operator()(Creature* pCreature) {
        return pCreature->getName() == m_Creature->getName();
    }

private:
    Creature* m_Creature;
};

//////////////////////////////////////////////////////////////////////////////
// add PC
//
// Add a PC to the zone for the first time. Tells the other PCs nearby that a new
// creature appeared, and scans the surroundings for object information.
//////////////////////////////////////////////////////////////////////////////
void Zone::addPC(Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir)

{
    __BEGIN_DEBUG

    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    __BEGIN_PROFILE_ZONE("Z_ADD_PC")

    VariableManager& variables = de::gameContext().variables();

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    WarSystem& warSystem = de::gameContext().warSystem();

    TPOINT pt = findSuitablePosition(this, cx, cy, pCreature->getMoveMode());

    if (pt.x != -1) {
        pCreature->setLastTarget(0);

        // Send the chosen coordinates to the client.
        GCSetPosition gcSetPosition;
        gcSetPosition.setX(pt.x);
        gcSetPosition.setY(pt.y);
        gcSetPosition.setDir(dir);

        pCreature->getPlayer()->sendPacket(&gcSetPosition);

        // Set the creature's coordinates and direction.
        pCreature->setXYDir(pt.x, pt.y, dir);

        // Once a suitable tile is found, put the creature into the PC manager
        // and into the tile.
        m_pTiles[pt.x][pt.y].addCreature(pCreature);


        m_pPCManager->addCreature(pCreature);

        // Clear the Sanctuary flag if it is set.

        // On the family rate plan, grant the default option bonus.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
        if (pGamePlayer->isFamilyPayAvailable() && !pCreature->isFlag(Effect::EFFECT_CLASS_FAMILY_BONUS)) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            Assert(pPC != NULL);

            pPC->addDefaultOptionSet(DEFAULT_OPTION_SET_FAMILY_PAY);
            addSimpleCreatureEffect(pPC, Effect::EFFECT_CLASS_FAMILY_BONUS);
            pPC->setFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        }

        // If EFFECT_CLASS_INIT_ALL_STAT is set, call initAllStat and clear the flag.
        if (pCreature->isFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT)) {
            if (pCreature->isSlayer()) {
                Slayer* pInitSlayer = dynamic_cast<Slayer*>(pCreature);
                Assert(pInitSlayer != NULL);

                SLAYER_RECORD prev;
                pInitSlayer->getSlayerRecord(prev);
                pInitSlayer->initAllStat();
                pInitSlayer->sendModifyInfo(prev);
            } else if (pCreature->isVampire()) {
                Vampire* pInitVampire = dynamic_cast<Vampire*>(pCreature);
                Assert(pInitVampire != NULL);

                VAMPIRE_RECORD prev;
                pInitVampire->getVampireRecord(prev);
                pInitVampire->initAllStat();
                pInitVampire->sendModifyInfo(prev);
            } else if (pCreature->isOusters()) {
                Ousters* pInitOusters = dynamic_cast<Ousters*>(pCreature);
                Assert(pInitOusters != NULL);

                OUSTERS_RECORD prev;
                pInitOusters->getOustersRecord(prev);
                pInitOusters->initAllStat();
                pInitOusters->sendModifyInfo(prev);
            }

            pCreature->removeFlag(Effect::EFFECT_CLASS_INIT_ALL_STAT);
        }

        if (pCreature->isSlayer()) {
            ((Slayer*)pCreature)->sendRealWearingInfo();
            ((Slayer*)pCreature)->sendSlayerSkillInfo();
            ((Slayer*)pCreature)->sendTimeLimitItemInfo();
        } else if (pCreature->isVampire()) {
            ((Vampire*)pCreature)->sendRealWearingInfo();
            ((Vampire*)pCreature)->sendVampireSkillInfo();
            ((Vampire*)pCreature)->sendTimeLimitItemInfo();
        } else if (pCreature->isOusters()) {
            ((Ousters*)pCreature)->sendRealWearingInfo();
            ((Ousters*)pCreature)->sendOustersSkillInfo();
            ((Ousters*)pCreature)->sendTimeLimitItemInfo();
        }

        // send RankBonus
        ((PlayerCreature*)pCreature)->sendRankBonusInfo();
        ((PlayerCreature*)pCreature)->sendCurrentQuestInfo();
        ((PlayerCreature*)pCreature)->getQuestManager()->adjustQuestStatus();


        GCModifyInformation gcModifyInformation;

        if (pCreature->isSlayer()) {
            Slayer* pNewSlayer = dynamic_cast<Slayer*>(pCreature);
            Assert(pNewSlayer != NULL);

            Player* pPlayer = pNewSlayer->getPlayer();
            Assert(pPlayer != NULL);

            gcModifyInformation.addShortData(MODIFY_DEFENSE, pNewSlayer->getDefense());
            gcModifyInformation.addShortData(MODIFY_PROTECTION, pNewSlayer->getProtection());
            gcModifyInformation.addShortData(MODIFY_TOHIT, pNewSlayer->getToHit());
            gcModifyInformation.addShortData(MODIFY_MIN_DAMAGE, pNewSlayer->getDamage(ATTR_CURRENT));
            gcModifyInformation.addShortData(MODIFY_MAX_DAMAGE, pNewSlayer->getDamage(ATTR_MAX));

            pPlayer->sendPacket(&gcModifyInformation);
        } else if (pCreature->isVampire()) {
            Vampire* pNewVampire = dynamic_cast<Vampire*>(pCreature);
            Assert(pNewVampire != NULL);

            Player* pPlayer = pNewVampire->getPlayer();
            Assert(pPlayer != NULL);

            gcModifyInformation.addShortData(MODIFY_DEFENSE, pNewVampire->getDefense());
            gcModifyInformation.addShortData(MODIFY_PROTECTION, pNewVampire->getProtection());
            gcModifyInformation.addShortData(MODIFY_TOHIT, pNewVampire->getToHit());
            gcModifyInformation.addShortData(MODIFY_MIN_DAMAGE, pNewVampire->getDamage(ATTR_CURRENT));
            gcModifyInformation.addShortData(MODIFY_MAX_DAMAGE, pNewVampire->getDamage(ATTR_MAX));

            pPlayer->sendPacket(&gcModifyInformation);
        } else if (pCreature->isOusters()) {
            Ousters* pNewOusters = dynamic_cast<Ousters*>(pCreature);
            Assert(pNewOusters != NULL);

            Player* pPlayer = pNewOusters->getPlayer();
            Assert(pPlayer != NULL);

            gcModifyInformation.addShortData(MODIFY_DEFENSE, pNewOusters->getDefense());
            gcModifyInformation.addShortData(MODIFY_PROTECTION, pNewOusters->getProtection());
            gcModifyInformation.addShortData(MODIFY_TOHIT, pNewOusters->getToHit());
            gcModifyInformation.addShortData(MODIFY_MIN_DAMAGE, pNewOusters->getDamage(ATTR_CURRENT));
            gcModifyInformation.addShortData(MODIFY_MAX_DAMAGE, pNewOusters->getDamage(ATTR_MAX));

            pPlayer->sendPacket(&gcModifyInformation);
        }

        //////////////////////////////////////////////////////////////////////////////
        // Send the pending message, if there is one.
        // Currently disabled.
        //////////////////////////////////////////////////////////////////////////////
        if (!pCreature->isFlag(Effect::EFFECT_CLASS_LOGIN_GUILD_MESSAGE)) {
            vector<string> messages = defaultMessageRepository().loadMessages(pCreature->getName());

            for (size_t m = 0; m < messages.size(); m++) {
                GCSystemMessage message;
                message.setMessage(messages[m]);
                pCreature->getPlayer()->sendPacket(&message);
            }

            defaultMessageRepository().deleteMessages(pCreature->getName());

            pCreature->setFlag(Effect::EFFECT_CLASS_LOGIN_GUILD_MESSAGE);
        }

        //////////////////////////////////////////////////////////////////////////////
        // Tell the client when PREMIUM_HALF_EVENT is on and this is a pay zone.
        //////////////////////////////////////////////////////////////////////////////
        if (variables.getVariable(PREMIUM_HALF_EVENT) && (m_ZoneID == 61 || m_ZoneID == 64 || m_ZoneID == 1007)) {
            GCNoticeEvent gcNoticeEvent;
            gcNoticeEvent.setCode(NOTICE_EVENT_PREMIUM_HALF_START);

            pCreature->getPlayer()->sendPacket(&gcNoticeEvent);
        }

        // Build the GCAddSlayer or GCAddVampire packet sent to nearby PCs.
        Creature::CreatureClass CClass = pCreature->getCreatureClass();
        if (CClass == Creature::CREATURE_CLASS_SLAYER) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            GCAddSlayer gcAddSlayer;
            makeGCAddSlayer(&gcAddSlayer, pSlayer);

            scan(pCreature, pt.x, pt.y, &gcAddSlayer);

            // A character with an attribute of 40 or more is expelled from the field headquarters.
            checkNewbieTransportToGuild(pSlayer);
        } else if (CClass == Creature::CREATURE_CLASS_VAMPIRE) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            GCAddVampire gcAddVampire;
            makeGCAddVampire(&gcAddVampire, pVampire);

            scan(pCreature, pt.x, pt.y, &gcAddVampire);

            // A vampire may have arrived through a portal, so
            // clear the flag.
            if (pVampire->isFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL)) {
                pVampire->removeFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL);
            }
        } else if (CClass == Creature::CREATURE_CLASS_OUSTERS) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            GCAddOusters gcAddOusters;
            makeGCAddOusters(&gcAddOusters, pOusters);

            scan(pCreature, pt.x, pt.y, &gcAddOusters);
        } else {
            throw Error("invalid creature class. must be slayer or vampire...");
        }

        // If the creature belongs to a party, join it to the local party.
        uint PartyID = pCreature->getPartyID();
        if (PartyID != 0) {
            // With a party, just add the member.
            m_pLocalPartyManager->addPartyMember(PartyID, pCreature);
        }

        // Pillar of fire.
        if (isMasterLair() && m_pMasterLairManager != NULL) {
            MasterLairInfo* pInfo = de::gameContext().masterLairInfos().getMasterLairInfo(getZoneID());
            Assert(pInfo != NULL);

            if (m_pMasterLairManager->getCurrentEvent() == MasterLairManager::EVENT_WAITING_PLAYER) {
                // Announce it when the continual pillar-of-fire effect is present.
                if (m_pEffectManager->findEffect(Effect::EFFECT_CLASS_CONTINUAL_GROUND_ATTACK) != NULL) {
                    GCNoticeEvent gcNoticeEvent;
                    gcNoticeEvent.setCode(NOTICE_EVENT_CONTINUAL_GROUND_ATTACK);
                    gcNoticeEvent.setParameter(pInfo->getStartDelay()); // seconds

                    broadcastPacket(&gcNoticeEvent);
                }
            }
        }

        //-----------------------------------------------------------------
        // When tax applies.
        //-----------------------------------------------------------------

        //-----------------------------------------------------------------
        // While a war is running, send the war information.
        //-----------------------------------------------------------------
        if (warSystem.isWarActive()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            warSystem.sendGCWarList(pPC->getPlayer());
        }

        if (de::gameContext().flags().hasFlagWar() && de::gameContext().flags().isFlagAllowedZone(getZoneID())) {
            Player* pPlayer = pCreature->getPlayer();
            if (pPlayer != NULL) {
                GCFlagWarStatus status = de::gameContext().flags().statusPacket();
                pPlayer->sendPacket(&status);
            }
        }

        if (m_pLevelWarManager != NULL && m_pLevelWarManager->hasWar()) {
            // During a level war the war list has to be sent.
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
            m_pLevelWarManager->sendGCWarList(pPC->getPlayer());
        }

        //-----------------------------------------------------------------
        // Entering Adam's holy land broadcasts an effect.
        //-----------------------------------------------------------------
        // The previous zone probably ought to be checked here.
        //-----------------------------------------------------------------
        sendHolyLandWarpEffect(pCreature);
        //}

        if (isHolyLand()) {
            if (warSystem.hasActiveRaceWar()) {
                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
                de::gameContext().shrines().sendBloodBibleStatus(pPC);

                GCRegenZoneStatus regenZoneStatus = RegenZoneManager::getInstance()->getStatus();
                pPC->getPlayer()->sendPacket(&regenZoneStatus);
            } else {
                GCHolyLandBonusInfo gcHolyLandBonusInfo;
                de::gameContext().bloodBibleBonuses().makeHolyLandBonusInfo(gcHolyLandBonusInfo);
                pCreature->getPlayer()->sendPacket(&gcHolyLandBonusInfo);
            }
        }

        if (de::gameContext().sweeperBonuses().isAble(getZoneID()) &&
            de::gameContext().levelWarZones().isCreatureBonusZone(pCreature, getZoneID())) {
            GCSweeperBonusInfo gcSweeperBonusInfo;
            de::gameContext().sweeperBonuses().makeSweeperBonusInfo(gcSweeperBonusInfo);
            pCreature->getPlayer()->sendPacket(&gcSweeperBonusInfo);
        }


        // Send the GCItemNameInfoList packet to the player.

        // A player revived after dying in a PK zone gets the resurrection effect.
        if (pCreature->isFlag(Effect::EFFECT_CLASS_PK_ZONE_RESURRECTION)) {
            Effect* pEffect = pCreature->findEffect(Effect::EFFECT_CLASS_PK_ZONE_RESURRECTION);
            if (pEffect != NULL) {
                // When the effect expires, the packet that attaches the resurrection effect is sent.
                pEffect->setDeadline(0);
            } else {
                pCreature->removeFlag(Effect::EFFECT_CLASS_PK_ZONE_RESURRECTION);
            }
        }

        // A freshly created character gets some extra packets.
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        if (pPC->getFlagSet()->isOn(FLAGSET_NOT_JUST_CREATED)) {
            GCNoticeEvent gcNoticeEvent;
            gcNoticeEvent.setCode(NOTICE_EVENT_WELCOME_MESSAGE);
            pPC->getPlayer()->sendPacket(&gcNoticeEvent);

            pPC->getFlagSet()->turnOff(FLAGSET_NOT_JUST_CREATED);
            pPC->getFlagSet()->save(pPC->getName());

            GCNoticeEvent gcNoticeEvent2;
            gcNoticeEvent2.setCode(NOTICE_EVENT_LOGIN_JUST_NOW);
            gcNoticeEvent2.setParameter(variables.getHeadPriceBonus());

            pPC->getPlayer()->sendPacket(&gcNoticeEvent2);

            pPC->setFlag(Effect::EFFECT_CLASS_JUST_LOGIN);

            if (variables.getVariable(CHOBO_EVENT)) {
                pPC->getGQuestManager()->getGQuestInventory().saveOne(pPC->getName(), 13);
                pPC->getPlayer()->sendPacket(pPC->getGQuestManager()->getGQuestInventory().getInventoryPacket());
                gcNoticeEvent.setCode(NOTICE_EVENT_GIVE_PRESENT_1);
                pPC->getPlayer()->sendPacket(&gcNoticeEvent);
            }
        } else if (!pPC->isFlag(Effect::EFFECT_CLASS_JUST_LOGIN)) {
            // Comeback-event reminders.
            if (defaultComebackEventRepository().hasUnclaimedItem(pPC->getPlayer()->getID())) {
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_CAN_GET_EVENT_200501_COMBACK_ITEM);
                pPC->getPlayer()->sendPacket(&response);
            }

            if (defaultComebackEventRepository().hasUnclaimedPremiumItem(pPC->getPlayer()->getID())) {
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_CAN_GET_EVENT_200501_COMBACK_PREMIUM_ITEM);
                pPC->getPlayer()->sendPacket(&response);
            }

            if (defaultComebackEventRepository().hasUnclaimedRecommendItem(pPC->getPlayer()->getID())) {
                GCNPCResponse response;
                response.setCode(NPC_RESPONSE_SHOW_COMMON_MESSAGE_DIALOG);
                response.setParameter(YOU_CAN_GET_EVENT_200501_COMBACK_RECOMMEND_ITEM);
                pPC->getPlayer()->sendPacket(&response);
            }

            if (variables.getVariable(TODAY_IS_HOLYDAY)) {
                GCNoticeEvent gcNoticeEvent;
                gcNoticeEvent.setCode(NOTICE_EVENT_HOLYDAY);
                gcNoticeEvent.setParameter(variables.getVariable(TODAY_IS_HOLYDAY));

                pPC->getPlayer()->sendPacket(&gcNoticeEvent);
            }

            if (canEnterBeginnerZone(pPC)) {
                int year = VSDate::currentDate().year() - 2000;
                int month = VSDate::currentDate().month();
                int day = VSDate::currentDate().day();
                int hour = VSTime::currentTime().hour();
                GCNoticeEvent gcNoticeEvent;
                gcNoticeEvent.setCode(NOTICE_EVENT_ENTER_BEGINNER_ZONE);
                gcNoticeEvent.setParameter((year * 1000000) + (month * 10000) + (day * 100) + hour);
                pPC->getPlayer()->sendPacket(&gcNoticeEvent);
            }

            if (variables.isWarActive() && variables.isAutoStartRaceWar() && warSystem.isRaceWarToday()) {
                GCNoticeEvent gcNoticeEvent;
                gcNoticeEvent.setCode(NOTICE_EVENT_RACE_WAR_SOON);
                gcNoticeEvent.setParameter(warSystem.getRaceWarTimeParam());
                pPC->getPlayer()->sendPacket(&gcNoticeEvent);
            }

            if (variables.isActiveLevelWar()) {
                ZoneID_t levelWarZoneId = de::gameContext().levelWarZones().getCreatureZoneID(pCreature);

                if (levelWarZoneId != 1) {
                    Zone* pLevelZone = getZoneByZoneID(levelWarZoneId);
                    Assert(pLevelZone != NULL);

                    LevelWarManager* pLevelWarManager = pLevelZone->getLevelWarManager();
                    Assert(pLevelWarManager != NULL);

                    if (pLevelWarManager->hasToDayWar()) {
                        int year = VSDate::currentDate().year() - 2000;
                        int month = VSDate::currentDate().month();
                        int day = VSDate::currentDate().day();
                        int hour = VSTime::currentTime().hour();
                        int level = 0;
                        if (levelWarZoneId == 1131)
                            level = 1;
                        else if (levelWarZoneId == 1132)
                            level = 2;
                        else if (levelWarZoneId == 1133)
                            level = 3;
                        else if (levelWarZoneId == 1134)
                            level = 4;

                        GCNoticeEvent gcNoticeEvent;
                        gcNoticeEvent.setCode(NOTICE_EVENT_LEVEL_WAR_ARRANGED);
                        gcNoticeEvent.setParameter((level * 100000000) + (year * 1000000) + (month * 10000) +
                                                   (day * 100) + hour);
                        pPC->getPlayer()->sendPacket(&gcNoticeEvent);
                    }
                }
            }

            GCNoticeEvent gcNoticeEvent;
            gcNoticeEvent.setCode(NOTICE_EVENT_LOGIN_JUST_NOW);
            gcNoticeEvent.setParameter(variables.getHeadPriceBonus());

            pPC->getPlayer()->sendPacket(&gcNoticeEvent);

            pPC->setFlag(Effect::EFFECT_CLASS_JUST_LOGIN);
        }

        if (pPC->getPetInfo() != NULL)
            sendPetInfo(pGamePlayer);
        // Packet sent on a zone change.
        pPC->getPlayer()->sendPacket(pPC->getNicknameBook()->getNicknameBookListPacket().get());

        Packet* pGQuestPacket = pPC->getGQuestManager()->getStatusInfoPacket();
        pPC->getPlayer()->sendPacket(pGQuestPacket);
        SAFE_DELETE(pGQuestPacket);

        pGQuestPacket = pPC->getGQuestManager()->getGQuestInventory().getInventoryPacket();
        pPC->getPlayer()->sendPacket(pGQuestPacket);

        // GCUnionOfferList
        //

        GCUnionOfferList gcUnionOfferList;

        {
            GuildUnion* pUnion = GuildUnionManager::Instance().getGuildUnion(pPC->getGuildID());
            if (pUnion != NULL) {
                if (de::gameContext().guilds().isGuildMaster(pPC->getGuildID(), pPC))

                    if (pUnion->getMasterGuildID() == pPC->getGuildID())

                        // Is the requester the master of its own guild, and is the union's master guild this guild?
                        if (de::gameContext().guilds().isGuildMaster(pPC->getGuildID(), pPC) &&
                            pUnion->getMasterGuildID() == pPC->getGuildID()) {
                            if (GuildUnionOfferManager::Instance().makeOfferList(pUnion->getUnionID(),
                                                                                 gcUnionOfferList)) {
                                pPC->getPlayer()->sendPacket(&gcUnionOfferList);
                            }
                        }
            }
        }

        if (warSystem.isSkyBlack()) {
            GCNoticeEvent gcNE;
            gcNE.setCode(NOTICE_EVENT_RACE_WAR_IN_5);
            pPC->getPlayer()->sendPacket(&gcNE);
        }

        GCMyStoreInfo myStoreInfo;
        myStoreInfo.setStoreInfo(&(pPC->getStore()->getStoreInfo()));
        myStoreInfo.setOpenUI(0);
        pPC->getPlayer()->sendPacket(&myStoreInfo);


    } else {
        ZoneCoord_t tempX = Random(20, m_Width);
        ZoneCoord_t tempY = Random(20, m_Height);
        addPC(pCreature, tempX, tempY, 0);

        // Assert when nothing was found within the maximum count.
    }


    __END_PROFILE_ZONE("Z_ADD_PC")

    __END_DEBUG
}

//--------------------------------------------------------------------------------
// add Creature
// When a creature first enters the zone, tell the PCs around it that it appeared.
//--------------------------------------------------------------------------------
void Zone::addCreature(Creature* pCreature, ZoneCoord_t cx, ZoneCoord_t cy, Dir_t dir)

{
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    __BEGIN_PROFILE_ZONE("Z_ADD_CREATURE")

    TPOINT pt = findSuitablePosition(this, cx, cy, pCreature->getMoveMode());

    // Check whether a spot was found.
    if (pt.x != -1) {
        //--------------------------------------------------------------------------------
        // Assign an OID.
        //--------------------------------------------------------------------------------
        m_ObjectRegistry.registerObject(pCreature);

        //--------------------------------------------------------------------------------
        // Once a suitable tile is found, put the creature into its creature manager and the tile.
        // A monster goes to the MonsterManager, an NPC to the NPCManager.
        //--------------------------------------------------------------------------------
        if (pCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            if (isDynamicZone()) {
                pMonster->setScanEnemy(true);
            }

            m_pMonsterManager->addCreature(pCreature);
            if (pCreature->getClanType() != CLAN_VAMPIRE_MONSTER)
                cout << pCreature->toString() << " regens at " << pt.x << " , " << pt.y << endl;

            switch (pMonster->getMonsterType()) {
            case 717:
            case 721:
            case 723:
            case 724:
            case 725: {
                unordered_map<ObjectID_t, Creature*>::iterator itr = m_pPCManager->getCreatures().begin();
                unordered_map<ObjectID_t, Creature*>::iterator endItr = m_pPCManager->getCreatures().end();

                for (; itr != endItr; ++itr) {
                    pMonster->addPotentialEnemy(itr->second);
                }
                break;
            }
            default:
                break;
            }


        } else if (pCreature->isNPC()) {
            m_pNPCManager->addCreature(pCreature);
        }

        m_pTiles[pt.x][pt.y].addCreature(pCreature, false);

        //--------------------------------------------------------------------------------
        // Set the creature's coordinates.
        //--------------------------------------------------------------------------------
        pCreature->setXYDir(pt.x, pt.y, dir);
        pCreature->setZone(this);


        //--------------------------------------------------------------------------------
        // Build the GCAddNPC or GCAddMonster packet sent to nearby PCs.
        //--------------------------------------------------------------------------------
        Creature::CreatureClass CClass = pCreature->getCreatureClass();

        if (CClass == Creature::CREATURE_CLASS_NPC) {
            NPC* pNPC = dynamic_cast<NPC*>(pCreature);
            GCAddNPC gcAddNPC;
            makeGCAddNPC(&gcAddNPC, pNPC);
            broadcastPacket(pt.x, pt.y, &gcAddNPC);
        } else if (CClass == Creature::CREATURE_CLASS_MONSTER) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            // A creature entering a zone can already be in various states.
            Packet* pAddMonsterPacket = createMonsterAddPacket(pMonster, NULL);

            if (pAddMonsterPacket != NULL) {
                broadcastPacket(cx, cy, pAddMonsterPacket, pMonster);
                delete pAddMonsterPacket;
            }

            // by sigi. 2002.9.6
            // Show the appearance of arriving through a portal and
            // clear the flag.
            if (pMonster->isFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL)) {
                pMonster->removeFlag(Effect::EFFECT_CLASS_VAMPIRE_PORTAL);
            }
        } else {
            throw Error("invalid creature type");
        }
    } else {
        throw EmptyTileNotExistException("too many creature in this zone.. or too unlucky");
    }

    __END_PROFILE_ZONE("Z_ADD_CREATURE")
}

//--------------------------------------------------------------------------------
// Delete PC from PC Manager (only do this)
//--------------------------------------------------------------------------------
void Zone::deletePC(Creature* pCreature)
// NoSuchElementException, Error)
{
    Assert(pCreature != NULL);
    m_pPCManager->deleteCreature(pCreature->getObjectID());
}

//--------------------------------------------------------------------------------
// Delete Queue PC
//--------------------------------------------------------------------------------
void Zone::deleteQueuePC(Creature* pCreature)

{
    __ENTER_CRITICAL_SECTION(m_Mutex)

    Assert(pCreature != NULL);

    list<Creature*>::iterator itr = find_if(m_PCListQueue.begin(), m_PCListQueue.end(), isSameCreature(pCreature));

    if (itr != m_PCListQueue.end()) {
        m_PCListQueue.erase(itr);
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

//--------------------------------------------------------------------------------
// Add PC to PC Manager (only do this)
//--------------------------------------------------------------------------------
void Zone::addPC(Creature* pCreature)

{
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    Assert(pCreature != NULL);
    m_pPCManager->addCreature(pCreature);
}

//--------------------------------------------------------------------------------
// Replace a PC with another PC object
//
// Used when a character is rebuilt as a different race: pFrom is taken off the
// tile it stands on and out of the PC manager, then pTo is put on the tile at
// (nx, ny), given that position and dir, and put into the PC manager. The
// destination may be the tile pFrom stood on.
//
// With bFindSuitablePosition set, (nx, ny) is only the starting point of a
// search for the nearest tile that is free for pTo's move mode and carries no
// portal. The search runs after pFrom has been removed, so the tile it vacated
// is itself a candidate.
//
// bCheckEffect and bCheckPortal are handed to Tile::addCreature: they decide
// whether the destination tile's effects (poison, darkness, trying position)
// are applied to pTo, and whether a portal on that tile is activated for it.
//
// The caller keeps everything else: broadcasting the swap, sending the new
// character's info, updating scans and disposing of pFrom.
//--------------------------------------------------------------------------------
void Zone::replacePC(Creature* pFrom, Creature* pTo, ZoneCoord_t nx, ZoneCoord_t ny, Dir_t dir,
                     bool bFindSuitablePosition, bool bCheckEffect, bool bCheckPortal) {
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    Assert(pFrom != NULL);
    Assert(pTo != NULL);

    getTile(pFrom->getX(), pFrom->getY()).deleteCreature(pFrom->getObjectID());
    deletePC(pFrom);

    if (bFindSuitablePosition) {
        TPOINT pt = findSuitablePosition(this, nx, ny, pTo->getMoveMode());
        nx = pt.x;
        ny = pt.y;
    }

    getTile(nx, ny).addCreature(pTo, bCheckEffect, bCheckPortal);
    pTo->setXYDir(nx, ny, dir);
    addPC(pTo);
}

//--------------------------------------------------------------------------------
// Put a creature on the tile at (x, y), and nothing else.
//
// The creature manager membership, the creature's own coordinates and move
// mode, and every broadcast stay with the caller. A caller that puts a
// creature into the zone as a whole wants addPC/addCreature instead; this is
// for the two cases that live below them: re-adding a creature to a tile to
// change the move mode it is filed under, and moving a creature between tiles
// without disturbing the manager that owns it.
//
// bCheckEffect and bCheckPortal are handed to Tile::addCreature: they decide
// whether the tile's effects (poison, darkness, trying position) are applied
// to the creature, and whether a portal on the tile is activated for it.
// Returns false when such a portal was activated, which has already carried
// the creature on to the portal's destination.
//--------------------------------------------------------------------------------
bool Zone::addCreatureToTile(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y, bool bCheckEffect, bool bCheckPortal) {
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    Assert(pCreature != NULL);

    return getTile(x, y).addCreature(pCreature, bCheckEffect, bCheckPortal);
}

//--------------------------------------------------------------------------------
// Take a creature off the tile at (x, y), and nothing else.
//
// The counterpart of addCreatureToTile: no creature manager is touched, so a
// creature removed this way is off the map but still owned by its manager,
// still heartbeaten, and still reachable by name or id. That is what the
// corpse paths want -- dropping a dead player from the PC manager would stop
// its effect manager's heartbeat. A caller that wants the creature out of the
// zone entirely wants deleteCreature instead.
//
// A creature that is not on that tile is a no-op: Tile::deleteCreature swallows
// the mismatch, logging it to tileError.txt when the tile is empty outright.
//--------------------------------------------------------------------------------
void Zone::deleteCreatureFromTile(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y) {
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    Assert(pCreature != NULL);

    getTile(x, y).deleteCreature(pCreature->getObjectID());
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void Zone::deleteCreature(Creature* pCreature, ZoneCoord_t x, ZoneCoord_t y)

{
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    __BEGIN_PROFILE_ZONE("Z_DELETE_CREATURE")

    try {
        Assert(pCreature->getX() == x && pCreature->getY() == y);

        // Remove the creature from its CreatureManager.
        if (pCreature->isPC()) {
            m_pPCManager->deleteCreature(pCreature->getObjectID());


            // Cancel a pending party invitation and delete its PartyInviteInfo.
            m_pPartyInviteInfoManager->cancelInvite(pCreature);

            // If the creature was in a party, remove it from the local party.
            uint PartyID = pCreature->getPartyID();
            if (PartyID != 0) {
                m_pLocalPartyManager->deletePartyMember(PartyID, pCreature);
            }

            // If a trade was in progress, delete the trade information.
            TradeInfo* pInfo = m_pTradeManager->getTradeInfo(pCreature->getName());
            if (pInfo != NULL) {
                m_pTradeManager->cancelTrade(pCreature);
            }
        } else if (pCreature->isMonster()) {
            m_pMonsterManager->deleteCreature(pCreature->getObjectID());
        } else if (pCreature->isNPC()) {
            m_pNPCManager->deleteCreature(pCreature->getObjectID());
        }

        // Remove the creature from the tile.
        try {
            getTile(x, y).deleteCreature(pCreature->getObjectID());
        } catch (NoSuchElementException& nsee) {
            // by sigi. 2002.12.10
            // When a player character dies:
            // [1] PCManager::killCreature() removes it from the tile and sets the target zone;
            // [2] EventResurrect hands it to IncomingPlayer, which puts it into the right zone.
            // Between the two, while it is still in the ZonePlayerManager, a transport caused by
            // something like the pay state would break the removal from the tile.
            // The pay check in the ZPM only runs in GPS_NORMAL for that reason, and the rest
            // is considered harmless, so only a log is written.
            filelog("zoneDeleteCreatureError.log", "%s", nsee.toString().c_str());
        }

        // Broadcast to nearby PCs that the creature is gone.
        GCDeleteObject gcDeleteObject(pCreature->getObjectID());
        broadcastPacket(x, y, &gcDeleteObject, pCreature);
    } catch (Throwable& t) {
        cerr << t.toString() << endl;
        filelog("zoneDeleteCreatureError.log", "Zone::deleteCreature() : %s", t.toString().c_str());
    }

    __END_PROFILE_ZONE("Z_DELETE_CREATURE")
}

//--------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------
void Zone::deleteObject(Object* pObject, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_PROFILE_ZONE("Z_DELETE_OBJECT")

    //--------------------------------------------------
    // Delete the object from the zone.
    //--------------------------------------------------
    getTile(x, y).deleteObject(pObject->getObjectID());

    //--------------------------------------------------
    // Broadcast to nearby PCs that the object is gone.
    //--------------------------------------------------
    GCDeleteObject gcDeleteObject(pObject->getObjectID());

    broadcastPacket(x, y, &gcDeleteObject);

    __END_PROFILE_ZONE("Z_DELETE_OBJECT")
}

//-------------------------------------------------------------
// create MonsterAddPacket
//-------------------------------------------------------------
// Build the GCAddXXX packet according to the monster's state.
//-------------------------------------------------------------
Packet* Zone::createMonsterAddPacket(Monster* pMonster, Creature* pPC) const

{
    Assert(pMonster != NULL);

    // When no viewer is given, everything is treated as visible.
    // The packet is built first and the visibility is checked
    // afterwards.
    if (pPC != NULL && !canSee(pPC, pMonster))
        return NULL;

    // Fetch the ObservingEye effect.
    //		//Assert( pEffectObservingEye );

    if (pMonster->isFlag(Effect::EFFECT_CLASS_HIDE)) {
        // A vampire, or anyone that can see it.
        {
            GCAddBurrowingCreature* pPacket = new GCAddBurrowingCreature();

            pPacket->setObjectID(pMonster->getObjectID());
            pPacket->setName(pMonster->getName());
            pPacket->setX(pMonster->getX());
            pPacket->setY(pMonster->getY());

            return pPacket;
        }

    }
    // In bat form.
    else if (pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT)) {
        GCAddBat* pPacket = new GCAddBat();
        pPacket->setObjectID(pMonster->getObjectID());
        pPacket->setName(pMonster->getName());
        pPacket->setXYDir(pMonster->getX(), pMonster->getY(), pMonster->getDir());
        pPacket->setItemType(0); // not used yet
        pPacket->setMaxHP(pMonster->getHP(ATTR_MAX));
        pPacket->setCurrentHP(pMonster->getHP(ATTR_CURRENT));
        pPacket->setGuildID(1);
        pPacket->setColor(0);

        return pPacket;
    }
    // In wolf form.
    else if (pMonster->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        GCAddWolf* pPacket = new GCAddWolf();
        pPacket->setObjectID(pMonster->getObjectID());
        pPacket->setName(pMonster->getName());
        pPacket->setXYDir(pMonster->getX(), pMonster->getY(), pMonster->getDir());
        pPacket->setItemType(0); // not used yet
        pPacket->setMaxHP(pMonster->getHP(ATTR_MAX));
        pPacket->setCurrentHP(pMonster->getHP(ATTR_CURRENT));
        pPacket->setGuildID(1);

        return pPacket;
    }
    // In the invisibility state.
    else if (pMonster->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
        // Visible? A vampire, or anyone that can see it.
        {
            // FIXME
            // The appearance depends on the settings.
            GCAddMonster* pPacket = new GCAddMonster();
            makeGCAddMonster(pPacket, pMonster);
            pPacket->setEffectInfo(pMonster->getEffectInfo());

            return pPacket;
        }
    } else {
        GCAddMonster* pPacket = new GCAddMonster();
        makeGCAddMonster(pPacket, pMonster);

        return pPacket;
    }

    return NULL;
}

bool Zone::deleteNPC(Creature* pCreature)

{
    if (pCreature == NULL || !pCreature->isNPC())
        return false;

    try {
        deleteCreature(pCreature, pCreature->getX(), pCreature->getY());
        de::gameContext().playerCreatures().deleteNPC(pCreature->getName());

    } catch (NoSuchElementException) {
        cout << "NoSuchNPC : " << pCreature->getName().c_str() << ", (" << pCreature->getX() << ", "
             << pCreature->getY() << ")" << endl;

        return false;
    }

    NPC* pNPC = dynamic_cast<NPC*>(pCreature);
    removeNPCInfo(pNPC);

    SAFE_DELETE(pCreature);

    return true;
}

void Zone::deleteNPCs(Race_t race)

{
    const unordered_map<ObjectID_t, Creature*>& NPCs = m_pNPCManager->getCreatures();
    unordered_map<ObjectID_t, Creature*>::const_iterator itr = NPCs.begin();

    list<ObjectID_t> creatures;

    // Collect the object IDs first.
    for (; itr != NPCs.end(); itr++) {
        Creature* pCreature = itr->second;
        creatures.push_back(pCreature->getObjectID());
    }

    list<ObjectID_t>::iterator oitr = creatures.begin();

    // Delete the NPCs.
    for (; oitr != creatures.end(); oitr++) {
        Creature* pCreature = m_pNPCManager->getCreature(*oitr);

        if (pCreature != NULL) {
            Assert(pCreature->isNPC());

            NPC* pNPC = dynamic_cast<NPC*>(pCreature);

            if (pNPC->getRace() == race) {
                deleteNPC(pCreature);
            }
        }
    }

    sendNPCInfo();
}

// Sets every monster's HP to 0 for its own AI to finish off. The war work
// that calls it runs on the zone's group thread (de::war::postToZones).
void Zone::killAllMonsters()

{
    if (m_pZoneGroup != NULL)
        m_pZoneGroup->assertOwned();

    __ENTER_CRITICAL_SECTION(m_Mutex)

    unordered_map<ObjectID_t, Creature*>& monsters = m_pMonsterManager->getCreatures();
    unordered_map<ObjectID_t, Creature*>::iterator itr = monsters.begin();

    for (; itr != monsters.end(); itr++) {
        Creature* pCreature = itr->second;
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        if (!pMonster->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE)) {
            pMonster->setHP(0);
        }
    }

    __LEAVE_CRITICAL_SECTION(m_Mutex)
}

void Zone::killAllMonsters_UNLOCK()

{
    unordered_map<ObjectID_t, Creature*>& monsters = m_pMonsterManager->getCreatures();
    unordered_map<ObjectID_t, Creature*>::iterator itr = monsters.begin();

    for (; itr != monsters.end(); itr++) {
        Creature* pCreature = itr->second;
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        if (!pMonster->isFlag(Effect::EFFECT_CLASS_NO_DAMAGE)) {
            pMonster->setHP(0);
        }
    }
}

void Zone::killAllPCs() {
    unordered_map<ObjectID_t, Creature*>& pcs = m_pPCManager->getCreatures();
    unordered_map<ObjectID_t, Creature*>::iterator itr = pcs.begin();

    for (; itr != pcs.end(); itr++) {
        Creature* pCreature = itr->second;

        if (pCreature->isSlayer()) {
            dynamic_cast<Slayer*>(pCreature)->setHP(0);
        } else if (pCreature->isVampire()) {
            dynamic_cast<Vampire*>(pCreature)->setHP(0);
        } else if (pCreature->isOusters()) {
            dynamic_cast<Ousters*>(pCreature)->setHP(0);
        }
    }
}
