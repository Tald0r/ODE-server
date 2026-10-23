#include "PartnerWaitingManager.h"

#include "GCNPCResponse.h"
#include "GameContext.h"
#include "PCFinder.h"
#include "PlayerCreature.h"
#include "VariableManager.h"
#include "WaitForApart.h"
#include "WaitForMeet.h"
#include "Zone.h"

PartnerWaitInfo::PartnerWaitInfo(PlayerCreature* pWaitingPC, string RequestedPCName) {
    Assert(pWaitingPC != NULL);

    m_pZone = pWaitingPC->getZone();
    Assert(m_pZone != NULL);

    m_RequestedPCName = RequestedPCName;
    m_WaitingPCOID = pWaitingPC->getObjectID();

    // a 1 minute limit
    Timeval currentTime;
    getCurrentTime(currentTime);
    m_Deadline = currentTime;
    m_Deadline.tv_sec += 60;
}

PartnerWaitInfo* PartnerWaitInfo::getPartnerWaitInfo(PlayerCreature* pWaitingPC, string RequestedPCName,
                                                     WaitType waitType) {
    Assert(pWaitingPC != NULL);

    switch (waitType) {
    case WAIT_FOR_MEET:
        return new WaitForMeet(pWaitingPC, RequestedPCName);
    case WAIT_FOR_APART:
        return new WaitForApart(pWaitingPC, RequestedPCName);
    default:
        return NULL;
    }
}

PlayerCreature* PartnerWaitInfo::getWaitingPC() const {
    Assert(m_pZone != NULL);

    Creature* pCreature = m_pZone->getCreature(m_WaitingPCOID);
    if (pCreature != NULL) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Assert(pPC != NULL);

        return pPC;
    }

    return NULL;
}

PartnerWaitingManager::~PartnerWaitingManager() {
    WaitInfoHashMap::iterator itr = m_WaitInfos.begin();

    for (; itr != m_WaitInfos.end(); itr++) {
        SAFE_DELETE(itr->second);
    }

    m_WaitInfos.clear();
}

// Build a new PartnerWaitInfo and register it.
uint PartnerWaitingManager::waitForPartner(PlayerCreature* pWaitingPC, string RequestedPCName) {
    if (!de::gameContext().variables().isActivateCouple())
        return COUPLE_MESSAGE_NOT_EVENT_TERM;

    Assert(pWaitingPC != NULL);
    if (isWaitForPartner(pWaitingPC))
        return COUPLE_MESSAGE_ALREADY_WAITING;

    PlayerCreature* pTargetPC = NULL;

    PCFinder& pcFinder = de::gameContext().playerCreatures();

    __ENTER_CRITICAL_SECTION(pcFinder)

    Creature* pTargetCreature = pcFinder.getCreature_LOCKED(RequestedPCName);
    if (pTargetCreature != NULL) {
        if (!pTargetCreature->isPC()) {
            return COUPLE_MESSAGE_LOGOFF;
        }

        pTargetPC = dynamic_cast<PlayerCreature*>(pTargetCreature);
        Assert(pTargetPC != NULL);
    }

    __LEAVE_CRITICAL_SECTION(pcFinder)

    if (pTargetPC == NULL)
        return COUPLE_MESSAGE_LOGOFF;

    // Build a new PartnerWaitInfo object through PartnerWaitInfo's FactoryMethod. WaitForMeet or WaitForApart
    PartnerWaitInfo* pPartnerWaitInfo = PartnerWaitInfo::getPartnerWaitInfo(pWaitingPC, RequestedPCName, getWaitType());
    Assert(pPartnerWaitInfo != NULL);

    uint result = pPartnerWaitInfo->waitPartner(pTargetPC);
    if (result != 0) {
        SAFE_DELETE(pPartnerWaitInfo);
        return result;
    }

    m_WaitInfos[RequestedPCName] = pPartnerWaitInfo;

    return 0;
}

// Find and erase the partner request that PC is waiting on.
bool PartnerWaitingManager::stopWaitForPartner(PlayerCreature* pWaitingPC) {
    Assert(pWaitingPC != NULL);

    WaitInfoHashMap::iterator itr = m_WaitInfos.begin();

    for (; itr != m_WaitInfos.end(); itr++) {
        if (pWaitingPC == itr->second->getWaitingPC()) {
            SAFE_DELETE(itr->second);
            m_WaitInfos.erase(itr);
            return true;
        }
    }

    return false;
}

uint PartnerWaitingManager::acceptPartner(PlayerCreature* pRequestedPC) {
    if (!de::gameContext().variables().isActivateCouple())
        return COUPLE_MESSAGE_NOT_EVENT_TERM;

    Assert(pRequestedPC != NULL);

    WaitInfoHashMap::iterator itr = m_WaitInfos.find(pRequestedPC->getName());
    // Nobody is waiting for this person.
    if (itr == m_WaitInfos.end())
        return COUPLE_MESSAGE_NO_WAITING;

    PartnerWaitInfo* pPartnerWaitInfo = itr->second;
    Assert(pPartnerWaitInfo != NULL);

    PlayerCreature* pWaitingPC = pPartnerWaitInfo->getWaitingPC();
    if (pWaitingPC == NULL)
        return COUPLE_MESSAGE_LOGOFF;

    // The same sex cannot be matched.
    if (pWaitingPC->getSex() == pRequestedPC->getSex())
        return COUPLE_MESSAGE_SAME_SEX;

    uint result = pPartnerWaitInfo->acceptPartner(pRequestedPC);

    // Erase it once it is matched.
    SAFE_DELETE(pPartnerWaitInfo);
    m_WaitInfos.erase(itr);

    return result;
}

bool PartnerWaitingManager::isWaitForPartner(PlayerCreature* pRequestedPC) {
    WaitInfoHashMap::iterator itr = m_WaitInfos.find(pRequestedPC->getName());
    return (itr != m_WaitInfos.end());
}

PlayerCreature* PartnerWaitingManager::getWaitingPartner(PlayerCreature* pRequestedPC) {
    WaitInfoHashMap::iterator itr = m_WaitInfos.find(pRequestedPC->getName());
    if (itr == m_WaitInfos.end())
        return NULL;

    return itr->second->getWaitingPC();
}

void PartnerWaitingManager::heartbeat() {
    WaitInfoHashMap::iterator itr = m_WaitInfos.begin();

    for (; itr != m_WaitInfos.end();) {
        PartnerWaitInfo* pPartnerWaitInfo = itr->second;
        Assert(pPartnerWaitInfo != NULL);

        Timeval currentTime;
        getCurrentTime(currentTime);

        if (pPartnerWaitInfo->getDeadline() < currentTime) {
            WaitInfoHashMap::iterator delitr = itr;
            itr++;

            // The deadline has passed!
            pPartnerWaitInfo->timeExpired();
            SAFE_DELETE(pPartnerWaitInfo);
            m_WaitInfos.erase(delitr);
        } else {
            itr++;
        }
    }
}
