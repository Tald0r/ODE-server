////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionWarpLevelWarZone.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionWarpLevelWarZone.h"

#include "Creature.h"
#include "GCSystemMessage.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "LevelWarManager.h"
#include "LevelWarZoneInfoManager.h"
#include "NPC.h"
#include "PacketUtil.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "StringPool.h"
#include "Utility.h"
#include "VariableManager.h"
#include "ZoneInfo.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
// read from PropertyBuffer
////////////////////////////////////////////////////////////////////////////////
void ActionWarpLevelWarZone::read(PropertyBuffer& propertyBuffer)

{}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionWarpLevelWarZone::execute(Creature* pCreature1, Creature* pCreature2)

{
    Assert(pCreature2 != NULL);
    Assert(pCreature2->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature2->getPlayer());
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature2);

    GCSystemMessage gcSystemMessage;


    // A character whose level is too high cannot enter.
    if (context().levelWarZones().getCreatureLevelGrade(pCreature2) == -1) {
        gcSystemMessage.setMessage(context().strings().getString(STRID_TO_HIGH_LEVEL_FOR_LEVEL_WAR));
        pGamePlayer->sendPacket(&gcSystemMessage);
        return;
    }

    // Pick the destination from the creature's information.
    ZONE_COORD pos(context().levelWarZones().getCreatureZoneID(pCreature2));

    if (pCreature2->isSlayer()) {
        pos.x = 12;
        pos.y = 9;
    } else if (pCreature2->isVampire()) {
        pos.x = 117;
        pos.y = 8;
    } else if (pCreature2->isOusters()) {
        pos.x = 9;
        pos.y = 111;
    }

    if (pCreature1 != NULL)
        pPC->getGQuestManager()->illegalWarp();
    transportCreature(pCreature2, pos.id, pos.x, pos.y, false);
    return;
}


////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionWarpLevelWarZone::toString() const

{
    StringStream msg;

    msg << "ActionWarpLevelWarZone(" << ")";

    return msg.toString();
}
