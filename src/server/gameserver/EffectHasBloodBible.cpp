//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHasBloodBible.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHasBloodBible.h"

#include "BloodBible.h"
#include "CastleInfoManager.h"
#include "Creature.h"
#include "GCAddEffect.h"
#include "GCBloodBibleStatus.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "HolyLandManager.h"
#include "ItemInfoManager.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "Player.h"
#include "ShrineInfoManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "WarSystem.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHasBloodBible::EffectHasBloodBible(Creature* pCreature)

    : EffectHasRelic(pCreature) {}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHasBloodBible::EffectHasBloodBible(Item* pItem)

    : EffectHasRelic(pItem) {}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasBloodBible::affect(Creature* pCreature)

{
    // Get the zone.
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    ShrineSet* pShrineSet = de::gameContext().shrines().getShrineSet(m_Part);
    Assert(pShrineSet != NULL);
    Race_t ShrineRace = pShrineSet->getOwnerRace();

    GCBloodBibleStatus* pGCBBS = new GCBloodBibleStatus;
    pGCBBS->setItemType(m_Part);
    pGCBBS->setZoneID(pZone->getZoneID());
    pGCBBS->setStorage(STORAGE_INVENTORY);
    pGCBBS->setOwnerName(pCreature->getName());
    pGCBBS->setRace(pCreature->getRace());
    pGCBBS->setShrineRace(ShrineRace);
    pGCBBS->setX(pCreature->getX());
    pGCBBS->setY(pCreature->getY());

    //	g_pHolyLandManager->broadcast( pGCBBS );
    de::gameContext().zoneGroups().broadcast(pGCBBS);
    de::gameContext().shrines().registerBloodBibleStatus(m_Part, pGCBBS);

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasBloodBible::affect(Item* pItem)

{
    if (m_pZone == NULL)
        return;

    ShrineSet* pShrineSet = de::gameContext().shrines().getShrineSet(m_Part);
    Assert(pShrineSet != NULL);

    GCBloodBibleStatus* pGCBBS = new GCBloodBibleStatus;
    pGCBBS->setItemType(m_Part);
    pGCBBS->setZoneID(m_pZone->getZoneID());
    pGCBBS->setStorage(STORAGE_CORPSE);
    // pGCBBS->setOwnerName( m_PartName );
    pGCBBS->setRace(pShrineSet->getOwnerRace());
    pGCBBS->setShrineRace(pShrineSet->getOwnerRace());
    pGCBBS->setX(m_X);
    pGCBBS->setY(m_Y);

    // Send the message only while a race war is running.
    if (de::gameContext().warSystem().hasActiveRaceWar()) {
        de::gameContext().holyLands().broadcast(pGCBBS);
    }

    de::gameContext().shrines().registerBloodBibleStatus(m_Part, pGCBBS);

    setNextTime(999999);
}

void EffectHasBloodBible::setPart(int part)

{
    const BloodBibleInfo* pBloodBibleInfo = dynamic_cast<const BloodBibleInfo*>(
        de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_BLOOD_BIBLE, part));

    if (pBloodBibleInfo != NULL) {
        m_Part = part;
        m_PartName = pBloodBibleInfo->getName();
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHasBloodBible::toString() const {
    StringStream msg;
    msg << "EffectHasBloodBible(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
