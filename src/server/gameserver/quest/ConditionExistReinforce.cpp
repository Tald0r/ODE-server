////////////////////////////////////////////////////////////////////////////////
// Filename    : ConditionExistReinforce.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ConditionExistReinforce.h"

#include "GameContext.h"
#include "Guild.h"
#include "GuildManager.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "war/SiegeWar.h"
#include "war/WarSchedule.h"
#include "war/WarScheduler.h"

////////////////////////////////////////////////////////////////////////////////
// is satisfied?
////////////////////////////////////////////////////////////////////////////////
bool ConditionExistReinforce::isSatisfied(Creature* pCreature1, Creature* pCreature2, void* pParam) const

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    Zone* pZone = getZoneByZoneID(m_ZoneID);
    Assert(pZone != NULL);
    Assert(pZone->isCastle());

    WarScheduler* pWarScheduler = pZone->getWarScheduler();
    Assert(pWarScheduler != NULL);

    Schedule* pNextSchedule = pWarScheduler->getRecentSchedule();

    Work* pNextWork = NULL;
    if (pNextSchedule != NULL)
        pNextWork = pNextSchedule->getWork();

    SiegeWar* pNextWar = dynamic_cast<SiegeWar*>(pNextWork);
    if (pNextWar == NULL)
        return false;

    GuildID_t gID = pNextWar->recentReinforceGuild();
    Guild* pGuild = de::gameContext().guilds().getGuild(gID);

    if (pGuild == NULL)
        return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
void ConditionExistReinforce::read(PropertyBuffer& propertyBuffer)

{
    m_ZoneID = propertyBuffer.getPropertyInt("ZoneID");
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ConditionExistReinforce::toString() const

{
    StringStream msg;
    msg << "ConditionExistReinforce(" << ")";
    return msg.toString();
}
