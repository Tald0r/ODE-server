/////////////////////////////////////////////////////////////////////////////
// DynamicZone.cpp
/////////////////////////////////////////////////////////////////////////////

// include files
#include "DynamicZone.h"

#include <stdio.h>

#include "DynamicZoneInfo.h"
#include "GameContext.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"

DynamicZone::DynamicZone() {
    m_pZone = NULL;
}

DynamicZone::~DynamicZone() {}

void DynamicZone::makeDynamicZone() {
    ZoneInfo* pTemplateZoneInfo = de::gameContext().zoneInfos().getZoneInfo(m_TemplateZoneID);
    Assert(pTemplateZoneInfo != NULL);

    char temp[128];

    // make zone info
    ZoneInfo* pZoneInfo = new ZoneInfo();
    pZoneInfo->setZoneID(m_ZoneID);
    pZoneInfo->setZoneGroupID(pTemplateZoneInfo->getZoneGroupID());
    pZoneInfo->setZoneType(pTemplateZoneInfo->getZoneType());
    pZoneInfo->setZoneLevel(pTemplateZoneInfo->getZoneLevel());
    pZoneInfo->setZoneAccessMode(pTemplateZoneInfo->getZoneAccessMode());
    pZoneInfo->setZoneOwnerID(pTemplateZoneInfo->getZoneOwnerID());
    pZoneInfo->setPayPlay(pTemplateZoneInfo->isPayPlay());
    pZoneInfo->setPremiumZone(pTemplateZoneInfo->isPremiumZone());
    pZoneInfo->setPKZone(pTemplateZoneInfo->isPKZone());
    pZoneInfo->setNoPortalZone(pTemplateZoneInfo->isNoPortalZone());
    pZoneInfo->setHolyLand(pTemplateZoneInfo->isHolyLand());
    pZoneInfo->setAvailable(pTemplateZoneInfo->isAvailable());
    pZoneInfo->setOpenLevel(pTemplateZoneInfo->getOpenLevel());
    pZoneInfo->setSMPFilename(pTemplateZoneInfo->getSMPFilename());
    pZoneInfo->setSSIFilename(pTemplateZoneInfo->getSSIFilename());
    snprintf(temp, sizeof(temp), "%s%u", pTemplateZoneInfo->getFullName().c_str(), m_ZoneID);
    pZoneInfo->setFullName(temp);
    snprintf(temp, sizeof(temp), "%s%u", pTemplateZoneInfo->getShortName().c_str(), m_ZoneID);
    pZoneInfo->setShortName(temp);

    // add zone info to the zone info table
    de::gameContext().zoneInfos().addZoneInfo(pZoneInfo);

    // make zone and add to ZoneGroup
    Zone* pZone = new Zone(pZoneInfo->getZoneID());
    Assert(pZone != NULL);

    // DynamicZone set to Zone
    pZone->setDynamicZone(this);

    // m_pZone setting
    m_pZone = pZone;

    // init zone
    pZone->init();
    // init DynamicZone
    init();

    // set ZoneGroup
    ZoneGroup* pZoneGroup = de::gameContext().zoneGroups().getZoneGroup(pZoneInfo->getZoneGroupID());
    Assert(pZoneGroup != NULL);

    pZone->setZoneGroup(pZoneGroup);
    pZoneGroup->addZone(pZone);
}
