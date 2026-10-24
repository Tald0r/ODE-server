/////////////////////////////////////////////////////////////////////////////
// BroadcastFilter.cpp
/////////////////////////////////////////////////////////////////////////////

// include files
#include "BroadcastFilter.h"

#include "Creature.h"
#include "GamePlayer.h"

/////////////////////////////////////////////////////////////////////////////
// class BroadcastFilterRace
/////////////////////////////////////////////////////////////////////////////
bool BroadcastFilterRace::isSatisfy(GamePlayer* pGamePlayer)

{
    Assert(pGamePlayer != NULL);

    Creature* pCreature = pGamePlayer->getCreature();
    Assert(pCreature != NULL);

    return pCreature->getRace() == m_Race;
}

BroadcastFilter* BroadcastFilterRace::Clone()

{
    BroadcastFilter* pFilter = new BroadcastFilterRace(m_Race);

    return pFilter;
}
