//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHasVampireRelic.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHasVampireRelic.h"

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
EffectHasVampireRelic::EffectHasVampireRelic(Creature* pCreature)

    : EffectHasRelic(pCreature){__BEGIN_TRY

                                    __END_CATCH}
      //////////////////////////////////////////////////////////////////////////////
      //////////////////////////////////////////////////////////////////////////////
      EffectHasVampireRelic::EffectHasVampireRelic(Item * pItem)

    : EffectHasRelic(pItem) {
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasVampireRelic::affect(Creature* pCreature)

{
    __BEGIN_TRY

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
    snprintf(msg, sizeof(msg), strings.c_str(STRID_HAVING_VAMPIRE_RELIC), pCreature->getName().c_str(), race,
             pZoneInfo->getFullName().c_str(), (int)pCreature->getX(), (int)pCreature->getY());

    GCSystemMessage gcSystemMessage;
    gcSystemMessage.setMessage(msg);

    de::gameContext().zoneGroups().broadcast(&gcSystemMessage);

    setNextTime(m_Tick);

    __END_CATCH
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHasVampireRelic::affect(Item* pItem)

    {__BEGIN_TRY

         __END_CATCH}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHasVampireRelic::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectHasVampireRelic(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();

    __END_CATCH
}
