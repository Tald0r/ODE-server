////////////////////////////////////////////////////////////////////////////////
// Filename    : ActionEnterSiege.cpp
// Written By  :
// Description :
////////////////////////////////////////////////////////////////////////////////

#include "ActionEnterSiege.h"

#include <stdio.h>

#include "CastleInfoManager.h"
#include "CreatureUtil.h"
#include "EventTransport.h"
#include "GCModifyInformation.h"
#include "GCMoveOK.h"
#include "GCNPCResponse.h"
#include "GCSystemMessage.h"
#include "GCUpdateInfo.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GuildManager.h"
#include "PlayerCreature.h"
#include "SiegeManager.h"
#include "SiegeWar.h"
#include "StringPool.h"
#include "StringStream.h"
#include "WarSchedule.h"
#include "WarScheduler.h"
#include "WarSystem.h"
#include "Zone.h"
#include "ZoneUtil.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void ActionEnterSiege::read(PropertyBuffer& pb)

{
    try {
        m_ZoneID = pb.getPropertyInt("ZoneID");
    } catch (NoSuchElementException& nsee) {
        throw Error(nsee.toString());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Execute the action.
////////////////////////////////////////////////////////////////////////////////
void ActionEnterSiege::execute(Creature* pNPC, Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);
    Assert(pCreature->isPC());

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pCreature->getPlayer());
    if (!context().warSystem().hasCastleActiveWar(m_ZoneID)) {
        GCSystemMessage gcSM;
        gcSM.setMessage("You can enter only while a siege is under way.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    Assert(pPC != NULL);

    Zone* pZone = getZoneByZoneID(m_ZoneID);
    Assert(pZone != NULL);

    WarScheduler* pWS = pZone->getWarScheduler();
    Assert(pWS != NULL);

    ZoneID_t siegeZoneID = SiegeManager::Instance().getSiegeZoneID(m_ZoneID);
    Assert(siegeZoneID != 0);


    // The siege is asked under the war system's lock: the main thread frees
    // it there when it ends, which may have happened since the test above.
    int side = 0;
    if (!context().warSystem().getSiegeGuildSide(m_ZoneID, pPC->getGuildID(), side)) {
        GCSystemMessage gcSM;
        gcSM.setMessage("An error occurred on server 1; please contact the operator.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    if (side == 0) {
        GCSystemMessage gcSM;
        gcSM.setMessage("Your guild has not applied for this war.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    if (!context().guilds().isGuildMaster(pPC->getGuildID(), pPC)) {
        GCSystemMessage gcSM;
        gcSM.setMessage("Only the guild master can apply.");
        pGamePlayer->sendPacket(&gcSM);
        return;
    }

    static TPOINT targetPos[7] = {{172, 38}, {172, 38}, {20, 232}, {20, 232}, {20, 232}, {20, 232}, {20, 232}};

    // Siege zone and the entry point for the guild's side.
    ZoneID_t ZoneNum = siegeZoneID;
    Coord_t ZoneX = targetPos[side - 1].x;
    Coord_t ZoneY = targetPos[side - 1].y;

    for (int i = 0; i < 7; ++i) {
        deleteCreatureEffect(pPC, (Effect::EffectClass)(Effect::EFFECT_CLASS_SIEGE_DEFENDER + i));
    }

    if (side < 8 && side > 0) {
        cout << "side : " << side << endl;
        addSimpleCreatureEffect(pPC, (Effect::EffectClass)(Effect::EFFECT_CLASS_SIEGE_DEFENDER + side - 1));
    }

    EventTransport* pEvent = dynamic_cast<EventTransport*>(pGamePlayer->getEvent(Event::EVENT_CLASS_TRANSPORT));
    bool newEvent = false;
    if (pEvent == NULL) {
        pEvent = new EventTransport(pGamePlayer);
        newEvent = true;
    }

    pEvent->setTargetZone(ZoneNum, ZoneX, ZoneY);
    pEvent->setDeadline(0);

    if (newEvent)
        pGamePlayer->addEvent(pEvent);

    __END_DEBUG
}

////////////////////////////////////////////////////////////////////////////////
// get debug string
////////////////////////////////////////////////////////////////////////////////
string ActionEnterSiege::toString() const

{
    StringStream msg;
    msg << "ActionEnterSiege(" << ")";
    return msg.toString();
}
