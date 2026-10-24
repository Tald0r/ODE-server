//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHasSlayerRelic.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHasSlayerRelic.h"

#include <stdio.h>

#include "Creature.h"
#include "GCAddEffect.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCStatusCurrentHP.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "Monster.h"
#include "MonsterCorpse.h"
#include "Player.h"
#include "Slayer.h"
#include "StringPool.h"
#include "Vampire.h"
#include "ZoneGroupManager.h"
#include "ZoneInfoManager.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHasSlayerRelic::EffectHasSlayerRelic(Creature* pCreature)

    : EffectHasRelic(pCreature) {}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHasSlayerRelic::EffectHasSlayerRelic(Item* pItem)

    : EffectHasRelic(pItem) {}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasSlayerRelic::affect(Creature* pCreature)

{
    StringPool& strings = de::gameContext().strings();

    // Get the zone.
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    ZoneInfo* pZoneInfo = de::gameContext().zoneInfos().getZoneInfo(pZone->getZoneID());
    Assert(pZoneInfo != NULL);

    // Announce the holder, their race and where they stand. The buffer holds
    // the longest message a GCSystemMessage carries.
    char msg[de::wire::kMaxByteStringLength + 1];

    const char* race = "";
    if (pCreature->isSlayer()) {
        race = strings.c_str(STRID_SLAYER);
    } else if (pCreature->isVampire()) {
        race = strings.c_str(STRID_VAMPIRE);
    } else if (pCreature->isOusters()) {
        race = strings.c_str(STRID_OUSTERS);
    }

    // The format is "<name> (<race>) holds the ... Relic at <zone> (<x>, <y>).".
    snprintf(msg, sizeof(msg), strings.c_str(STRID_HAVING_SLAYER_RELIC), pCreature->getName().c_str(), race,
             pZoneInfo->getFullName().c_str(), (int)pCreature->getX(), (int)pCreature->getY());

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg);

    de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasSlayerRelic::affect(Item* pItem)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHasSlayerRelic::toString() const {
    StringStream msg;
    msg << "EffectHasSlayerRelic(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
