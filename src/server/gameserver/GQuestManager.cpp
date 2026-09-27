#include "GQuestManager.h"

#include <cstdio>

#include "EffectEventQuestReset.h"
#include "GCGQuestStatusInfo.h"
#include "GCGQuestStatusModify.h"
#include "GCSystemMessage.h"
#include "GQuestAllElements.h"
#include "GQuestCheckPoint.h"
#include "GQuestInfo.h"
#include "GQuestStatus.h"
#include "GameContext.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "MonsterInfo.h"
#include "NPC.h"
#include "Party.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "SXml.h"
#include "Timeval.h"
#include "repository/PlayRecordRepository.h"

void GQuestManager::load()

{
    __BEGIN_TRY

    vector<SavedQuestRow> rows = defaultPlayRecordRepository().loadSavedQuests(m_pOwner->getName());

    for (size_t r = 0; r < rows.size(); r++) {
        WORD qID = rows[r].questID;
        BYTE sta = rows[r].status;

        if (sta != QuestStatusInfo::COMPLETE && sta != QuestStatusInfo::FAIL && sta != QuestStatusInfo::CAN_REPLAY) {
            filelog("GQuestError.log", "Saved quest has an invalid status : [%s]:%d/%d", m_pOwner->getName().c_str(),
                    qID, sta);
        } else {
            if (sta == QuestStatusInfo::CAN_REPLAY)
                continue;

            m_QuestStatuses[qID] = new GQuestStatus(m_pOwner, qID);
            m_QuestStatuses[qID]->setStatus(sta);
        }

        if ((qID == 1001 || qID == 2001 || qID == 3001) && sta == QuestStatusInfo::COMPLETE) {
            cout << "complete.." << endl;
            EffectEventQuestReset* pEffect = new EffectEventQuestReset(m_pOwner, 1);
            int lastSec = rows[r].secondsSinceSave;
            if (lastSec > EVENT_QUEST_TIME_LIMIT)
                lastSec = EVENT_QUEST_TIME_LIMIT;
            cout << "Elapsed lastSec : " << lastSec << endl;
            pEffect->setDeadline((EVENT_QUEST_TIME_LIMIT - lastSec) * 10);
            cout << "Deadline : " << (Turn_t)((EVENT_QUEST_TIME_LIMIT - lastSec) * 10) << endl;
            pEffect->setNextTime(((EVENT_QUEST_TIME_LIMIT - lastSec) % BROADCASTING_DELAY) * 10);
            m_pOwner->addEffect(pEffect);
        }
    }

    __END_CATCH
}

void GQuestManager::init()

{
    __BEGIN_TRY

    load();
    m_GQuestInventory.load(m_pOwner->getName());
    refreshQuest(false);

    __END_CATCH
}

void GQuestManager::refreshQuest(bool sendPacket) {
    __BEGIN_TRY

    unordered_map<DWORD, GQuestInfo*>& infos = GQuestInfoManager::Instance().getInfos();

    unordered_map<DWORD, GQuestInfo*>::iterator itr = infos.begin();
    unordered_map<DWORD, GQuestInfo*>::iterator endItr = infos.end();

    for (; itr != endItr; ++itr) {
        //		if ( m_QuestStatuses[itr->first] != NULL ) continue;
        unordered_map<DWORD, GQuestStatus*>::iterator finditr = m_QuestStatuses.find(itr->first);
        if (finditr != m_QuestStatuses.end()) {
            if (finditr->second->getStatus() == QuestStatusInfo::CAN_ACCEPT) {
                if (!itr->second->isInstanceSuccess(GQuestInfo::HAPPEN, m_pOwner)) {
                    m_QuestStatuses.erase(finditr);
                    continue;
                }
            } else {
                continue;
            }
        }

        if (itr->second->isInstanceSuccess(GQuestInfo::HAPPEN, m_pOwner)) {
            m_QuestStatuses[itr->first] = itr->second->makeInitStatus(m_pOwner);
        }
    }

    if (sendPacket) {
        m_pOwner->getPlayer()->sendPacket(getStatusInfoPacket());
        m_pOwner->getPlayer()->sendPacket(m_GQuestInventory.getInventoryPacket());
    }

    __END_CATCH
}

Packet* GQuestManager::getStatusInfoPacket() const {
    GCGQuestStatusInfo* pRet = new GCGQuestStatusInfo;

    unordered_map<DWORD, GQuestStatus*>::const_iterator itr = m_QuestStatuses.begin();
    unordered_map<DWORD, GQuestStatus*>::const_iterator endItr = m_QuestStatuses.end();

    for (; itr != endItr; ++itr) {
        if (itr->second != NULL)
            pRet->getInfos().push_back(itr->second);
    }

    return pRet;
}

void GQuestManager::accept(DWORD qID) {
    unordered_map<DWORD, GQuestStatus*>::iterator itr = m_QuestStatuses.find(qID);
    if (itr == m_QuestStatuses.end()) {
        cout << "accept : no such quest " << qID << endl;
        return;
    }

    //	GQuestStatus* pQS = m_QuestStatuses[qID];
    GQuestStatus* pQS = itr->second;
    if (pQS == NULL) {
        cout << "accept : null status " << qID << endl;
        return;
    }
    if (pQS->getStatus() != QuestStatusInfo::CAN_ACCEPT && pQS->getStatus() != QuestStatusInfo::CAN_REPLAY) {
        cout << "accept : not CAN_ACCEPT " << (int)pQS->getStatus() << endl;
        return;
    }

    pQS->initMissions();
    pQS->setStatus(QuestStatusInfo::DOING);
    pQS->checkMissions();

    GCGQuestStatusModify gcModify;
    gcModify.setInfo(pQS);
    gcModify.setType(GCGQuestStatusModify::CURRENT);
    m_pOwner->getPlayer()->sendPacket(&gcModify);
}

void GQuestManager::cancel(DWORD qID) {
    unordered_map<DWORD, GQuestStatus*>::iterator itr = m_QuestStatuses.find(qID);
    if (itr == m_QuestStatuses.end()) {
        cout << "cancel : no such quest " << qID << endl;
        return;
    }

    //	GQuestStatus* pQS = m_QuestStatuses[qID];
    GQuestStatus* pQS = itr->second;
    if (pQS == NULL) {
        cout << "cancel : null status " << qID << endl;
        return;
    }
    if (pQS->getStatus() != QuestStatusInfo::DOING) {
        cout << "cancel : not DOING " << (int)pQS->getStatus() << endl;
        return;
    }

    pQS->setStatus(QuestStatusInfo::CAN_REPLAY);
    pQS->cleanUpMissions();

    GCGQuestStatusModify gcModify;
    gcModify.setInfo(pQS);
    gcModify.setType(GCGQuestStatusModify::FAIL);
    m_pOwner->getPlayer()->sendPacket(&gcModify);
}

void GQuestManager::heartbeat() {
    list<GQuestMission*>::iterator itr = m_EventMissions[TIMER].begin();
    list<GQuestMission*>::iterator endItr = m_EventMissions[TIMER].end();

    for (; itr != endItr; ++itr) {
        GQuestTimeMission* pTimeMission = dynamic_cast<GQuestTimeMission*>((*itr));
        if (pTimeMission == NULL)
            continue;

        pTimeMission->updateArg();
        Timeval endTime = pTimeMission->getEndTime();
        if (gCurrentTime > endTime) {
            pTimeMission->m_pParent->update();
            // Stopgap.
            break;
        }
    }
}

void GQuestManager::blooddrain() {
    list<GQuestMission*>::iterator itr = m_EventMissions[BLOODDRAIN].begin();
    while (itr != m_EventMissions[BLOODDRAIN].end()) {
        GQuestBloodDrainMission* pBloodDrainMission = dynamic_cast<GQuestBloodDrainMission*>((*itr));
        ++itr;
        if (pBloodDrainMission == NULL)
            continue;

        pBloodDrainMission->increase();
        char buffer[256];

        if (m_pOwner->isVampire()) {
            snprintf(buffer, sizeof(buffer), "Blood drained %u times.", pBloodDrainMission->getCurrent());
            GCSystemMessage gcSM;
            gcSM.setMessage(buffer);
            m_pOwner->getPlayer()->sendPacket(&gcSM);
        } else if (m_pOwner->isOusters()) {
            snprintf(buffer, sizeof(buffer), "Soul absorbed %u times.", pBloodDrainMission->getCurrent());
            GCSystemMessage gcSM;
            gcSM.setMessage(buffer);
            m_pOwner->getPlayer()->sendPacket(&gcSM);
        }

        if (pBloodDrainMission->getGoal() <= pBloodDrainMission->getCurrent()) {
            pBloodDrainMission->m_pParent->update();
        } else {
            GCGQuestStatusModify gcSM;
            gcSM.setInfo(pBloodDrainMission->m_pParent);
            gcSM.setType(GCGQuestStatusModify::NO_MODIFY);
            m_pOwner->getPlayer()->sendPacket(&gcSM);
        }

        // Stopgap.
        //		break;
    }
}

void GQuestManager::levelUp() {
    list<GQuestMission*>::iterator itr = m_EventMissions[LEVELUP].begin();
    while (itr != m_EventMissions[LEVELUP].end()) {
        GQuestLevelMission* pLevelMission = dynamic_cast<GQuestLevelMission*>((*itr));
        ++itr;
        if (pLevelMission == NULL)
            continue;

        if (pLevelMission->isSuccess(m_pOwner)) {
            pLevelMission->m_pParent->update();
            // Stopgap.
            //			break;
        }
    }

    refreshQuest();

    if (m_pOwner->getLevel() == 25 && m_pOwner->getPartyID() != 0) {
        Party* pParty = de::gameContext().parties().getParty(m_pOwner->getPartyID());
        if (pParty != NULL && pParty->getSize() == 2) {
            pParty->eventPartyCrash();
        }
    }
}

void GQuestManager::getItem(Item* pItem) {}

bool GQuestManager::metNPC(NPC* pNPC) {
    list<GQuestMission*>::iterator itr = m_EventMissions[MEETNPC].begin();
    while (itr != m_EventMissions[MEETNPC].end()) {
        GQuestSayNPCMission* pSayNPCMission = dynamic_cast<GQuestSayNPCMission*>((*itr));
        ++itr;
        if (pSayNPCMission == NULL)
            continue;

        GQuestSayNPCElement* pSayNPCElement = dynamic_cast<GQuestSayNPCElement*>(*pSayNPCMission->m_Position);
        if (pSayNPCElement == NULL) {
            cout << "SayNPCElement cast failed" << endl;
            Assert(false);
        }

        //		cout << "check SayNPCElement : " << pSayNPCElement->getTarget() << endl;

        if (pSayNPCElement->getTarget() == pNPC->getNPCID()) {
            pSayNPCMission->meet();
            pSayNPCMission->m_pParent->update();
            // Meet only one at a time.
            return true;
        }
    }

    return false;
}

void GQuestManager::tamePet(PetInfo* pPetInfo) {
    list<GQuestMission*>::iterator itr = m_EventMissions[TAMEPET].begin();
    while (itr != m_EventMissions[TAMEPET].end()) {
        GQuestTamePetMission* pTamePetMission = dynamic_cast<GQuestTamePetMission*>((*itr));
        ++itr;
        if (pTamePetMission == NULL)
            continue;

        pTamePetMission->tame();
        pTamePetMission->m_pParent->update();
    }
}

void GQuestManager::killed() {
    list<GQuestMission*>::iterator itr = m_EventMissions[KILLED].begin();
    while (itr != m_EventMissions[KILLED].end()) {
        GQuestKilledMission* pKilledMission = dynamic_cast<GQuestKilledMission*>((*itr));
        ++itr;
        if (pKilledMission == NULL)
            continue;

        pKilledMission->increase();

        if (pKilledMission->getGoal() <= pKilledMission->getCurrent()) {
            pKilledMission->m_pParent->update();
        }

        // Stopgap.
        //		break;
    }
}

void GQuestManager::skillLevelUp(SkillSlot* pSkillSlot) {
    list<GQuestMission*>::iterator itr = m_EventMissions[SKILL_LEVELUP].begin();
    while (itr != m_EventMissions[SKILL_LEVELUP].end()) {
        GQuestSkillLevelMission* pSkillLevelMission = dynamic_cast<GQuestSkillLevelMission*>((*itr));
        ++itr;
        if (pSkillLevelMission == NULL)
            continue;

        if (pSkillLevelMission->isSuccess(pSkillSlot)) {
            pSkillLevelMission->m_pParent->update();
        }
    }
}

void GQuestManager::rideMotorcycle(bool isParty) {
    list<GQuestMission*>::iterator itr = m_EventMissions[RIDE_MOTORCYCLE].begin();
    while (itr != m_EventMissions[RIDE_MOTORCYCLE].end()) {
        GQuestRideMotorcycleMission* pRideMotorcycleMission = dynamic_cast<GQuestRideMotorcycleMission*>((*itr));
        ++itr;
        if (pRideMotorcycleMission == NULL)
            continue;

        pRideMotorcycleMission->ride();

        if (pRideMotorcycleMission->isRide()) {
            pRideMotorcycleMission->m_pParent->update();
        }

        // Stopgap.
        //		break;
    }

    if (!isParty && m_pOwner->getPartyID() != 0) {
        Party* pParty = de::gameContext().parties().getParty(m_pOwner->getPartyID());
        if (pParty != NULL && pParty->getSize() == 2) {
            unordered_map<string, Creature*> members = pParty->getMemberMap();
            unordered_map<string, Creature*>::iterator itr = members.begin();

            for (; itr != members.end(); ++itr) {
                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->lock();
                }

                PlayerCreature* pMember = dynamic_cast<PlayerCreature*>(itr->second);
                pMember->getGQuestManager()->rideMotorcycle(true);

                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->unlock();
                }
            }
        }
    }
}

void GQuestManager::touchWayPoint(MonsterCorpse* pWayPoint) {
    cout << "touchWayPoint : " << m_pOwner->getName() << endl;
    list<GQuestMission*>::iterator itr = m_EventMissions[TOUCH_WAY_POINT].begin();
    while (itr != m_EventMissions[TOUCH_WAY_POINT].end()) {
        GQuestTouchWayPointMission* pTouchWayPointMission = dynamic_cast<GQuestTouchWayPointMission*>((*itr));
        ++itr;
        if (pTouchWayPointMission == NULL)
            continue;

        GQuestTouchWayPointElement* pTouchWayPointElement =
            dynamic_cast<GQuestTouchWayPointElement*>(*pTouchWayPointMission->m_Position);
        if (pTouchWayPointElement == NULL) {
            cout << "TouchWayPointElement cast failed" << endl;
            Assert(false);
        }

        if (pTouchWayPointElement->m_ZoneID == m_pOwner->getZoneID() &&
            pTouchWayPointElement->m_X == pWayPoint->getX() && pTouchWayPointElement->m_Y == pWayPoint->getY()) {
            if (pTouchWayPointElement->m_Type != pWayPoint->getMonsterType())
                cout << "Way point monster type does not match" << endl;
            pTouchWayPointMission->touch();

            pTouchWayPointMission->m_pParent->update();
        }
    }

    if (m_pOwner->getPartyID() != 0) {
        Party* pParty = de::gameContext().parties().getParty(m_pOwner->getPartyID());
        if (pParty != NULL && pParty->getSize() == 2) {
            unordered_map<string, Creature*> members = pParty->getMemberMap();
            unordered_map<string, Creature*>::iterator itr = members.begin();

            for (; itr != members.end(); ++itr) {
                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->lock();
                }

                PlayerCreature* pMember = dynamic_cast<PlayerCreature*>(itr->second);
                pMember->getGQuestManager()->partyTravel(pWayPoint);

                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->unlock();
                }
            }
        }
    }
}

void GQuestManager::killedMonster(Monster* pMonster) {
    list<GQuestMission*>::iterator itr = m_EventMissions[KILLMONSTER].begin();
    while (itr != m_EventMissions[KILLMONSTER].end()) {
        GQuestKillMonsterMission* pKillMonsterMission = dynamic_cast<GQuestKillMonsterMission*>((*itr));
        ++itr;
        if (pKillMonsterMission == NULL)
            continue;

        if (pKillMonsterMission->isTarget(pMonster->getSpriteType())) {
            pKillMonsterMission->increase();
            GQuestKillMonsterElement* pKillMonsterElement =
                dynamic_cast<GQuestKillMonsterElement*>(*pKillMonsterMission->m_Position);
            if (pKillMonsterElement == NULL) {
                cout << "KillMonsterElement cast failed" << endl;
                Assert(false);
            }
            if (pKillMonsterElement->getGoal() <= pKillMonsterMission->getCurrent()) {
                pKillMonsterMission->m_pParent->update();
            }
        }
    }
}

void GQuestManager::partyDissect(MonsterCorpse* pMonsterCorpse) {
    cout << "partyDissect : " << endl;
    list<GQuestMission*>::iterator itr = m_EventMissions[PARTY_DISSECT].begin();
    while (itr != m_EventMissions[PARTY_DISSECT].end()) {
        GQuestPartyDissectMission* pPartyDissectMission = dynamic_cast<GQuestPartyDissectMission*>((*itr));
        ++itr;
        if (pPartyDissectMission == NULL)
            continue;

        cout << "Target : " << pPartyDissectMission->m_StrArg << endl;
        cout << "Number : " << (int)pPartyDissectMission->getTargetList().front() << endl;
        cout << "Killed : " << pMonsterCorpse->getMonsterType() << endl;

        if (pPartyDissectMission->isTarget(
                de::gameContext().monsterInfos().getMonsterInfo(pMonsterCorpse->getMonsterType())->getSpriteType())) {
            GQuestPartyDissectElement* pPartyDissectElement =
                dynamic_cast<GQuestPartyDissectElement*>(*pPartyDissectMission->m_Position);
            if (pPartyDissectElement == NULL) {
                cout << "PartyDissectElement cast failed" << endl;
                Assert(false);
            }
            pPartyDissectMission->increase();
            cout << "partyDissect : +1" << endl;
            if (pPartyDissectElement->getGoal() <= pPartyDissectMission->getCurrent()) {
                pPartyDissectMission->m_pParent->update();
            } else {
                GCGQuestStatusModify gcSM;
                gcSM.setInfo(pPartyDissectMission->m_pParent);
                gcSM.setType(GCGQuestStatusModify::NO_MODIFY);
                m_pOwner->getPlayer()->sendPacket(&gcSM);
            }
        }
    }
}

void GQuestManager::eventParty() {
    m_bPartyQuest = true;
    cout << "Event party formed. : " << m_pOwner->getName() << endl;
    list<GQuestMission*>::iterator itr = m_EventMissions[EVENT_PARTY].begin();
    while (itr != m_EventMissions[EVENT_PARTY].end()) {
        GQuestEventPartyMission* pEventPartyMission = dynamic_cast<GQuestEventPartyMission*>((*itr));
        ++itr;
        if (pEventPartyMission == NULL)
            continue;

        GQuestEventPartyElement* pEventPartyElement =
            dynamic_cast<GQuestEventPartyElement*>(*pEventPartyMission->m_Position);
        if (pEventPartyElement == NULL) {
            cout << "EventPartyElement cast failed" << endl;
            Assert(false);
        }

        //		cout << "check EventPartyElement : " << pEventPartyElement->getTarget() << endl;

        pEventPartyMission->meet();
        pEventPartyMission->m_pParent->update();
    }
}

void GQuestManager::eventPartyCrash() {
    m_bPartyQuest = false;
    cout << "Event party broken. : " << m_pOwner->getName() << endl;
    list<GQuestMission*>::iterator itr = m_EventMissions[EVENT_PARTY_CRASH].begin();
    while (itr != m_EventMissions[EVENT_PARTY_CRASH].end()) {
        GQuestEventPartyCrashMission* pEventPartyCrashMission = dynamic_cast<GQuestEventPartyCrashMission*>((*itr));
        ++itr;
        if (pEventPartyCrashMission == NULL)
            continue;

        GQuestEventPartyCrashElement* pEventPartyCrashElement =
            dynamic_cast<GQuestEventPartyCrashElement*>(*pEventPartyCrashMission->m_Position);
        if (pEventPartyCrashElement == NULL) {
            cout << "EventPartyCrashElement cast failed" << endl;
            Assert(false);
        }

        //		cout << "check EventPartyCrashElement : " << pEventPartyCrashElement->getTarget() << endl;

        pEventPartyCrashMission->meet();
        pEventPartyCrashMission->m_pParent->update();
    }
}

void GQuestManager::fastMove(bool isParty) {
    list<GQuestMission*>::iterator itr = m_EventMissions[FASTMOVE].begin();
    while (itr != m_EventMissions[FASTMOVE].end()) {
        GQuestFastMoveMission* pFastMoveMission = dynamic_cast<GQuestFastMoveMission*>((*itr));
        ++itr;
        if (pFastMoveMission == NULL)
            continue;

        pFastMoveMission->ride();

        if (pFastMoveMission->isRide()) {
            pFastMoveMission->m_pParent->update();
        }
    }

    if (!isParty && m_pOwner->getPartyID() != 0) {
        Party* pParty = de::gameContext().parties().getParty(m_pOwner->getPartyID());
        if (pParty != NULL && pParty->getSize() == 2) {
            unordered_map<string, Creature*> members = pParty->getMemberMap();
            unordered_map<string, Creature*>::iterator itr = members.begin();

            for (; itr != members.end(); ++itr) {
                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->lock();
                }

                PlayerCreature* pMember = dynamic_cast<PlayerCreature*>(itr->second);
                pMember->getGQuestManager()->fastMove(true);

                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->unlock();
                }
            }
        }
    }
}

void GQuestManager::illegalWarp(bool isParty) {
    list<GQuestMission*>::iterator itr = m_EventMissions[ILLEGAL_WARP].begin();
    while (itr != m_EventMissions[ILLEGAL_WARP].end()) {
        GQuestIllegalWarpMission* pIllegalWarpMission = dynamic_cast<GQuestIllegalWarpMission*>((*itr));
        ++itr;
        if (pIllegalWarpMission == NULL)
            continue;

        pIllegalWarpMission->ride();

        if (pIllegalWarpMission->isRide()) {
            pIllegalWarpMission->m_pParent->update();
        }
    }

    if (!isParty && m_pOwner->getPartyID() != 0) {
        Party* pParty = de::gameContext().parties().getParty(m_pOwner->getPartyID());
        if (pParty != NULL && pParty->getSize() == 2) {
            unordered_map<string, Creature*> members = pParty->getMemberMap();
            unordered_map<string, Creature*>::iterator itr = members.begin();

            for (; itr != members.end(); ++itr) {
                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->lock();
                }

                PlayerCreature* pMember = dynamic_cast<PlayerCreature*>(itr->second);
                pMember->getGQuestManager()->illegalWarp(true);

                if (m_pOwner->getZoneID() != itr->second->getZoneID()) {
                    itr->second->getZone()->unlock();
                }
            }
        }
    }
}

void GQuestManager::partyTravel(MonsterCorpse* pCorpse) {
    cout << "partyTravel : " << m_pOwner->getName();
    DWORD id = GQuestCheckPoint::Instance().getCheckPointID(pCorpse);
    if (id == 0)
        return;

    list<GQuestMission*>::iterator itr = m_EventMissions[TRAVEL].begin();
    while (itr != m_EventMissions[TRAVEL].end()) {
        GQuestTravelMission* pTravelMission = dynamic_cast<GQuestTravelMission*>((*itr));
        ++itr;
        if (pTravelMission == NULL)
            continue;

        if (pTravelMission->isTarget(id) && !pTravelMission->isVisited(id)) {
            pTravelMission->increase();
            pTravelMission->getVisitedList().push_back(id);
            pTravelMission->updateStr();
            pTravelMission->m_pParent->update();
        }
    }
}

void GQuestManager::advancementClassLevelUp() {
    list<GQuestMission*>::iterator itr = m_EventMissions[ADVANCEMENT_LEVELUP].begin();
    while (itr != m_EventMissions[ADVANCEMENT_LEVELUP].end()) {
        GQuestAdvancementClassLevelMission* pAdvancementClassLevelMission =
            dynamic_cast<GQuestAdvancementClassLevelMission*>((*itr));
        ++itr;
        if (pAdvancementClassLevelMission == NULL)
            continue;

        if (pAdvancementClassLevelMission->isSuccess(m_pOwner)) {
            pAdvancementClassLevelMission->m_pParent->update();
            // Stopgap.
            //			break;
        }
    }

    refreshQuest();
}

void GQuestManager::clearDynamicZone(ZoneID_t zoneID) {
    list<GQuestMission*>::iterator itr = m_EventMissions[CLEAR_DYNAMIC_ZONE].begin();
    while (itr != m_EventMissions[CLEAR_DYNAMIC_ZONE].end()) {
        GQuestClearDynamicZoneMission* pClearDynamicZoneMission = dynamic_cast<GQuestClearDynamicZoneMission*>((*itr));
        ++itr;
        if (pClearDynamicZoneMission == NULL)
            continue;
        pClearDynamicZoneMission->clear(zoneID);

        if (pClearDynamicZoneMission->isClear()) {
            pClearDynamicZoneMission->m_pParent->update();
            // Stopgap.
            //			break;
        }
    }
}

void GQuestManager::enterDynamicZone(ZoneID_t zoneID) {
    list<GQuestMission*>::iterator itr = m_EventMissions[ENTER_DYNAMIC_ZONE].begin();
    while (itr != m_EventMissions[ENTER_DYNAMIC_ZONE].end()) {
        GQuestEnterDynamicZoneMission* pEnterDynamicZoneMission = dynamic_cast<GQuestEnterDynamicZoneMission*>((*itr));
        ++itr;
        if (pEnterDynamicZoneMission == NULL)
            continue;
        pEnterDynamicZoneMission->enter(zoneID);

        if (pEnterDynamicZoneMission->isEnter()) {
            pEnterDynamicZoneMission->m_pParent->update();
            // Stopgap.
            //			break;
        }
    }
}

GQuestStatus* GQuestManager::getGQuestStatus(DWORD qID) {
    unordered_map<DWORD, GQuestStatus*>::iterator itr = m_QuestStatuses.find(qID);

    if (itr == m_QuestStatuses.end())
        return NULL;

    return itr->second;
}

void GQuestManager::eraseQuest(DWORD qID) {
    unordered_map<DWORD, GQuestStatus*>::iterator itr = m_QuestStatuses.find(qID);
    if (itr != m_QuestStatuses.end())
        m_QuestStatuses.erase(itr);

    defaultPlayRecordRepository().deleteSavedQuest(m_pOwner->getName(), qID);
}
