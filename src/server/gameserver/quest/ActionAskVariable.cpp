////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionAskVariable.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionAskVariable.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "GCNPCAskVariable.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Guild.h"
#include "GuildManager.h"
#include "MonsterInfo.h"
#include "NPC.h"
#include "Ousters.h"
#include "PlayerCreature.h"
#include "SiegeWar.h"
#include "Slayer.h"
#include "StringPool.h"
#include "StringStream.h"
#include "Vampire.h"
#include "VariableBuffer.h"
#include "VariableInfo.h"
#include "VariableManager.h"
#include "WarSchedule.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "ZoneUtil.h"

void convertCommaString(string& str);

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ActionAskVariable::read(PropertyBuffer& propertyBuffer)

{
    __BEGIN_TRY

    try {
        // read script id
        m_ScriptID = propertyBuffer.getPropertyInt("ScriptID");

        string buffer = propertyBuffer.getProperty("Variable");

        SAFE_DELETE(m_pVariableBuffer);

        m_pVariableBuffer = new VariableBuffer(buffer);
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionAskVariable::execute(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    StringPool& strings = context().strings();

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);
    Assert(pCreature1->isNPC());
    Assert(pCreature2->isPC());

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);
    Assert(pPC != NULL);

    CastleInfoManager& castleInfos = context().castleInfos();

    GCNPCAskVariable gcNPCAskVariable;
    gcNPCAskVariable.setObjectID(pCreature1->getObjectID());
    gcNPCAskVariable.setScriptID(m_ScriptID);

    int count = m_pVariableBuffer->getCount();

    for (int i = 0; i < count; i++) {
        VariableInfo* pInfo = m_pVariableBuffer->getVariableInfo(i);

        ScriptParameter* pParam = new ScriptParameter();
        pParam->setName(pInfo->getName());

        string keyword = pInfo->getKeyword();

        if (keyword == "EntranceFee") {
            // Fill in the entrance fee.
            ZoneID_t zoneID = atoi(pInfo->getParameter(0).c_str());

            if (zoneID == 0)
                throw Error("Invalid Script Variable. Keyword : EntranceFee. Invalid ZoneID");

            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

            Gold_t value = castleInfos.getEntranceFee(zoneID, pPC);
            Race_t race = castleInfos.getCastleInfo(zoneID)->getRace();

            char strValue[20];
            // Free for everyone during a race war.
            // During a guild war only the race allowed into the castle enters free.
            if (context().warSystem().hasActiveRaceWar() || context().warSystem().hasCastleActiveWar(zoneID)) {
                snprintf(strValue, sizeof(strValue), "%s", strings.getString(STRID_FREE).c_str());
            } else if (race == RACE_SLAYER) {
                char gold[15];
                snprintf(gold, sizeof(gold), "%u", value);
                string sGold(gold);
                convertCommaString(sGold);
                snprintf(strValue, sizeof(strValue), "%s", (sGold + " " + strings.getString(STRID_REI)).c_str());
            } else {
                char gold[15];
                snprintf(gold, sizeof(gold), "%u", value);
                string sGold(gold);
                convertCommaString(sGold);
                snprintf(strValue, sizeof(strValue), "%s", (sGold + " " + strings.getString(STRID_GELD)).c_str());
            }

            if (castleInfos.isPossibleEnter(zoneID, pPC))
                pParam->setValue(strValue);
            else
                pParam->setValue(strings.getString(STRID_NO_ENTER));
        } else if (keyword == "CastleOwner") {
            // Look up the castle owner and fill it in.
            ZoneID_t zoneID = atoi(pInfo->getParameter(0).c_str());

            if (zoneID == 0)
                throw Error("Invalid Script Variable. Keyword : CastleOwner. Invalid ZoneID");

            CastleInfo* pCastleInfo = castleInfos.getCastleInfo(zoneID);
            string result;
            if (pCastleInfo != NULL) {
                if (pCastleInfo->isCommon()) {
                    // A common castle.
                    if (pCastleInfo->getRace() == Guild::GUILD_RACE_SLAYER) {
                        // A Slayer common castle.
                        result = strings.getString(STRID_SLAYER_COMMON_CASTLE);
                    } else if (pCastleInfo->getRace() == Guild::GUILD_RACE_VAMPIRE) {
                        // A Vampire common castle.
                        result = strings.getString(STRID_VAMPIRE_COMMON_CASTLE);
                    } else {
                        result = strings.getString(STRID_OUSTERS_COMMON_CASTLE);
                    }
                } else {
                    // A castle owned by a guild.
                    Guild* pGuild = context().guilds().getGuild(pCastleInfo->getGuildID());
                    if (pGuild == NULL)
                        result = strings.getString(STRID_NO_MASTER_CASTLE);
                    else
                        //						result = pGuild->getName() + ( (pGuild->getRace() ==
                        // RACE_SLAYER)?"Team":"Clan" ) + "'s castle";
                        result = pGuild->getName() +
                                 ((pGuild->getRace() == RACE_SLAYER) ? (strings.getString(STRID_TEAM))
                                                                     : (strings.getString(STRID_CLAN))) +
                                 strings.getString(STRID_S_CASTLE);
                }

                pParam->setValue(result);
            } else {
                pParam->setValue("");
            }
        } else if (keyword == "CastleName") {
            ZoneID_t zoneID = atoi(pInfo->getParameter(0).c_str());

            if (zoneID == 0)
                throw Error("Invalid Script Variable. Keyword : CastleOwner. Invalid ZoneID");

            CastleInfo* pCastleInfo = castleInfos.getCastleInfo(zoneID);
            if (pCastleInfo != NULL) {
                pParam->setValue(pCastleInfo->getName());
            } else {
                pParam->setValue("");
            }
        } else if (keyword == "ReinforceCandidate") {
            ZoneID_t zoneID = atoi(pInfo->getParameter(0).c_str());

            Zone* pZone = getZoneByZoneID(zoneID);
            Assert(pZone != NULL);
            Assert(pZone->isCastle());

            WarScheduler* pWarScheduler = pZone->getWarScheduler();
            Assert(pWarScheduler != NULL);

            Schedule* pNextSchedule = pWarScheduler->getRecentSchedule();

            Work* pNextWork = NULL;
            if (pNextSchedule != NULL)
                pNextWork = pNextSchedule->getWork();

            SiegeWar* pNextWar = dynamic_cast<SiegeWar*>(pNextWork);

            if (pNextWar == NULL) {
                pParam->setValue("None");
            } else {
                GuildID_t gID = pNextWar->recentReinforceGuild();
                Guild* pGuild = context().guilds().getGuild(gID);
                if (pGuild == NULL)
                    pParam->setValue("None");
                else
                    pParam->setValue(pGuild->getName());

                cout << pParam->getValue() << " reinforcement" << endl;
                ;
            }
        } else if (keyword == "RedistGold") {
            Gold_t price = context().variables().getVariable(VAMPIRE_REDISTRIBUTE_ATTR_PRICE);

            char gold[15];
            snprintf(gold, sizeof(gold), "%u", price);

            string sGold(gold);
            convertCommaString(sGold);

            pParam->setValue(sGold);
        } else if (keyword == "CastleResurrectFee") {
            ZoneID_t zoneID = atoi(pInfo->getParameter(0).c_str());

            CastleInfo* pCastleInfo = castleInfos.getCastleInfo(zoneID);
            if (pCastleInfo != NULL) {
                Gold_t value = pCastleInfo->getEntranceFee();

                PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

                if (castleInfos.isCastleMember(zoneID, pPC))
                    value = 0;

                char gold[15];
                snprintf(gold, sizeof(gold), "%u", value);
                string sGold(gold);
                convertCommaString(sGold);

                char strValue[20];

                if (pCastleInfo->getRace() == RACE_SLAYER) {
                    snprintf(strValue, sizeof(strValue), "%s", (sGold + " " + strings.getString(STRID_REI)).c_str());
                } else if (pCastleInfo->getRace() == RACE_VAMPIRE) {
                    snprintf(strValue, sizeof(strValue), "%s", (sGold + " " + strings.getString(STRID_GELD)).c_str());
                } else {
                    snprintf(strValue, sizeof(strValue), "%s", (sGold + " " + strings.getString(STRID_ZARD)).c_str());
                }

                pParam->setValue(strValue);
            } else {
                pParam->setValue("");
            }
        } else if (keyword == "EventQuestZone") {
            int questLevel = atoi(pInfo->getParameter(0).c_str());

            if (questLevel == 2) {
                if (pCreature2->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
                    Attr_t grade = pSlayer->getQuestGrade();

                    if (grade < 61) {
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_1));
                    } else if (grade < 96) {
                        pParam->setValue("Eslania north-west");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_2));
                    } else if (grade < 131) {
                        pParam->setValue("Eslania north-east");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_3));
                    } else if (grade < 171) {
                        pParam->setValue("Eslania south-west");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_4));
                    } else if (grade < 211) {
                        pParam->setValue("Eslania dungeon");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_5));
                    } else if (grade < 241) {
                        pParam->setValue("Drobeta south-west");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_6));
                    } else if (grade < 271) {
                        pParam->setValue("Drobeta south-east");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_7));
                    } else if (grade < 291) {
                        pParam->setValue("Lake Timore south-west");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_8));
                    } else if (grade < 301) {
                        pParam->setValue("Lake Timore south-east");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_9));
                    } else {
                        pParam->setValue("Laom dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_2_10));
                    }
                } else if (pCreature2->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
                    Level_t level = pVampire->getLevel();

                    if (level < 11) {
                        pParam->setValue("Bathory dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_1));
                    } else if (level < 21) {
                        pParam->setValue("Limbo south-east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_2));
                    } else if (level < 31) {
                        pParam->setValue("Limbo north-east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_3));
                    } else if (level < 41) {
                        pParam->setValue("Limbo north-west");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_4));
                    } else if (level < 51) {
                        pParam->setValue("Lake Timore north-east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_5));
                    } else if (level < 61) {
                        pParam->setValue("Mount Rodin south-west");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_6));
                    } else if (level < 71) {
                        pParam->setValue("Mount Rodin south-east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_7));
                    } else if (level < 81) {
                        pParam->setValue("Icen dungeon floor 1");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_8));
                    } else if (level < 91) {
                        pParam->setValue("Icen dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_9));
                    } else {
                        pParam->setValue("Adam's holy land east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_2_10));
                    }
                } else if (pCreature2->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
                    Level_t level = pOusters->getLevel();

                    if (level < 11) {
                        pParam->setValue("Hanial dungeon floor 1");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_1));
                    } else if (level < 21) {
                        pParam->setValue("Hanial dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_2));
                    } else if (level < 31) {
                        pParam->setValue("Castalo north-east");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_3));
                    } else if (level < 41) {
                        pParam->setValue("Ghorgova tunnel");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_4));
                    } else if (level < 51) {
                        pParam->setValue("Drobeta north-east");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_5));
                    } else if (level < 61) {
                        pParam->setValue("Drobeta north-west");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_6));
                    } else if (level < 71) {
                        pParam->setValue("Mount Rodin north-east");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_7));
                    } else if (level < 81) {
                        pParam->setValue("Mount Rodin north-west");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_8));
                    } else if (level < 91) {
                        pParam->setValue("Rasen inner castle floor 2");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_9));
                    } else {
                        pParam->setValue("Laom dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_2_10));
                    }
                }
            } else if (questLevel == 3) {
                if (pCreature2->isSlayer()) {
                    pParam->setValue("Eslania south-west");
                    pParam->setValue(strings.getString(STRID_SLAYER_MINE_ENTER));
                } else if (pCreature2->isVampire()) {
                    pParam->setValue("Limbo north-east");
                    pParam->setValue(strings.getString(STRID_VAMPIRE_MINE_ENTER));
                } else if (pCreature2->isOusters()) {
                    pParam->setValue("Castalo north-east");
                    pParam->setValue(strings.getString(STRID_OUSTERS_MINE_ENTER));
                } else {
                    filelog("EventBug.txt",
                            "ActionAskVariable : stage 3 quest zone lookup got a player of no known race.");
                }
            } else if (questLevel == 4) {
                if (pCreature2->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
                    Attr_t grade = pSlayer->getQuestGrade();

                    if (grade < 131) {
                        pParam->setValue("Eslania north-west");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_4_1));
                    } else if (grade < 211) {
                        pParam->setValue("Eslania dungeon");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_4_2));
                    } else if (grade < 271) {
                        pParam->setValue("Drobeta south-east");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_4_3));
                    } else if (grade < 300) {
                        pParam->setValue("Timore south-east");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_4_4));
                    } else {
                        pParam->setValue("Laom dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_SLAYER_QUESTZONE_4_5));
                    }
                } else if (pCreature2->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
                    Level_t level = pVampire->getLevel();

                    if (level < 31) {
                        pParam->setValue("Limbo south-east");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_4_1));
                    } else if (level < 51) {
                        pParam->setValue("Limbo north-west");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_4_2));
                    } else if (level < 71) {
                        pParam->setValue("Drobeta south-west");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_4_3));
                    } else if (level < 91) {
                        pParam->setValue("Icen dungeon floor 1");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_4_4));
                    } else {
                        pParam->setValue("Icen dungeon floor 2");
                        pParam->setValue(strings.getString(STRID_VAMPIRE_QUESTZONE_4_5));
                    }
                } else if (pCreature2->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
                    Level_t level = pOusters->getLevel();

                    if (level < 31) {
                        pParam->setValue("Castalo north-east");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_4_1));
                    } else if (level < 51) {
                        pParam->setValue("Drobeta north-west");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_4_2));
                    } else if (level < 71) {
                        pParam->setValue("Mount Rodin south-west");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_4_3));
                    } else if (level < 91) {
                        pParam->setValue("Timore south-east");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_4_4));
                    } else {
                        pParam->setValue("Laom dungeon floor 1");
                        pParam->setValue(strings.getString(STRID_OUSTERS_QUESTZONE_4_5));
                    }
                } else {
                    filelog("EventBug.txt",
                            "ActionAskVariable : stage 4 quest zone lookup got a player of no known race.");
                }
            } else {
                filelog("EventBug.txt", "ActionAskVariable : quest zone lookup got an unknown quest level. %d",
                        questLevel);
                Assert(false);
            }
        } else if (keyword == "EventQuestMonster") {
            int questLevel = atoi(pInfo->getParameter(0).c_str());

            if (questLevel == 2) {
                if (pCreature2->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
                    Attr_t grade = pSlayer->getQuestGrade();

                    if (grade < 61) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIMAT));
                    } else if (grade < 96) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIMAT));
                    } else if (grade < 131) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIMAT));
                    } else if (grade < 171) {
                        pParam->setValue(strings.getString(STRID_RED_ZIMAT));
                    } else if (grade < 211) {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIMAT));
                    } else if (grade < 241) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIRCON));
                    } else if (grade < 271) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIRCON));
                    } else if (grade < 291) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIRCON));
                    } else if (grade < 301) {
                        pParam->setValue(strings.getString(STRID_RED_ZIRCON));
                    } else {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIRCON));
                    }
                } else if (pCreature2->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
                    Level_t level = pVampire->getLevel();

                    if (level < 11) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIMAT));
                    } else if (level < 21) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIMAT));
                    } else if (level < 31) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIMAT));
                    } else if (level < 41) {
                        pParam->setValue(strings.getString(STRID_RED_ZIMAT));
                    } else if (level < 51) {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIMAT));
                    } else if (level < 61) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIRCON));
                    } else if (level < 71) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIRCON));
                    } else if (level < 81) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIRCON));
                    } else if (level < 91) {
                        pParam->setValue(strings.getString(STRID_RED_ZIRCON));
                    } else {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIRCON));
                    }
                } else if (pCreature2->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
                    Level_t level = pOusters->getLevel();

                    if (level < 11) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIMAT));
                    } else if (level < 21) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIMAT));
                    } else if (level < 31) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIMAT));
                    } else if (level < 41) {
                        pParam->setValue(strings.getString(STRID_RED_ZIMAT));
                    } else if (level < 51) {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIMAT));
                    } else if (level < 61) {
                        pParam->setValue(strings.getString(STRID_YELLOW_ZIRCON));
                    } else if (level < 71) {
                        pParam->setValue(strings.getString(STRID_GREEN_ZIRCON));
                    } else if (level < 81) {
                        pParam->setValue(strings.getString(STRID_BLUE_ZIRCON));
                    } else if (level < 91) {
                        pParam->setValue(strings.getString(STRID_RED_ZIRCON));
                    } else {
                        pParam->setValue(strings.getString(STRID_BLACK_ZIRCON));
                    }
                }
            } else if (questLevel == 5) {
                if (pCreature2->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
                    Attr_t grade = pSlayer->getQuestGrade();

                    if (grade < 131) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_1));
                    } else if (grade < 211) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_2));
                    } else if (grade < 271) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_3));
                    } else if (grade < 301) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_4));
                    } else {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_5));
                    }
                } else if (pCreature2->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
                    Level_t level = pVampire->getLevel();

                    if (level < 31) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_1));
                    } else if (level < 51) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_2));
                    } else if (level < 71) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_3));
                    } else if (level < 91) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_4));
                    } else {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_5));
                    }
                } else if (pCreature2->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
                    Level_t level = pOusters->getLevel();

                    if (level < 31) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_1));
                    } else if (level < 51) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_2));
                    } else if (level < 71) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_3));
                    } else if (level < 91) {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_4));
                    } else {
                        pParam->setValue(strings.getString(STRID_QUEST_MONSTER_5));
                    }
                } else {
                    filelog("EventBug.txt",
                            "ActionAskVariable : stage 4 quest monster lookup got a player of no known race.");
                }
            } else {
                filelog("EventBug.txt", "ActionAskVariable : quest monster lookup got an unknown quest level. %d",
                        questLevel);
                Assert(false);
            }
        } else if (keyword == "ClearRankBonusFee") {
        } else if (keyword == "RacePetQuestTarget") {
            int level = 0;

            if (pCreature2->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature2);
                Assert(pSlayer != NULL);
                level = pSlayer->getHighestSkillDomainLevel();
            } else if (pCreature2->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature2);
                Assert(pVampire != NULL);
                level = pVampire->getLevel();
            } else if (pCreature2->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature2);
                Assert(pOusters != NULL);
                level = pOusters->getLevel();
            }

            if (level < 40) {
                GCSystemMessage gcSM;
                gcSM.setMessage("You cannot take the pet quest at your current level.");
                pCreature2->getPlayer()->sendPacket(&gcSM);
                GCNPCResponse gcNPCR;
                gcNPCR.setCode(NPC_RESPONSE_QUIT_DIALOGUE);
                pCreature2->getPlayer()->sendPacket(&gcNPCR);
                return;
            }

            pPC->initPetQuestTarget();
            // pPC->getTargetMonsterSType() ); 			if ( !mList.empty() )
            //			{
            const MonsterInfo* pMonsterInfo = context().monsterInfos().getMonsterInfo(pPC->getTargetMonsterSType());
            Assert(pMonsterInfo != NULL);

            pParam->setValue(pMonsterInfo->getHName());
            //			}
        } else if (keyword == "UserName") {
            pParam->setValue(pCreature2->getName());
        } else if (keyword == "GuildName") {
            pParam->setValue(pPC->getGuildName());
        }

        gcNPCAskVariable.addScriptParameter(pParam);
    }

    Player* pPlayer = pCreature2->getPlayer();
    pPlayer->sendPacket(&gcNPCAskVariable);

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionAskVariable::toString() const

{
    __BEGIN_TRY

    StringStream msg;
    msg << "ActionAskVariable(" << ",ScriptID:" << (int)m_ScriptID
        << ",VariableBuffer:" << m_pVariableBuffer->toString() << ")";

    return msg.toString();

    __END_CATCH
}

void convertCommaString(string& str) {
    int size = str.size();
    int l = (size - 1) / 3;

    for (int i = 0; i < l; i++) {
        str.insert(size - ((i + 1) * 3), ",");
    }
}
