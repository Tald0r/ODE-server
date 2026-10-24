//////////////////////////////////////////////////////////////////////////////
// Filename    : CGThrowItemHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGThrowItem.h"

#ifdef __GAME_SERVER__
#include "GCSkillFailed1.h"
#include "GCSkillFailed2.h"
#include "GCThrowItemOK1.h"
#include "GCThrowItemOK2.h"
#include "GCThrowItemOK3.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "SkillHandlerManager.h"
#include "Slayer.h"
#include "Vampire.h"
#include "Zone.h"
#include "ZoneUtil.h"
#include "item/HolyWater.h"
#include "skill/Sniping.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGThrowItemHandler::execute(CGThrowItem* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        // Just return if the game player's state is not normal.
        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

        Creature* pCreature = pGamePlayer->getCreature();
        // Just return if the player is not a Slayer.
        if (!pCreature->isSlayer())
            return;

        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        // A complete safe zone forbids skill use.
        ZoneLevel_t ZoneLevel = pSlayer->getZone()->getZoneLevel(pSlayer->getX(), pSlayer->getY());
        if (ZoneLevel & COMPLETE_SAFE_ZONE) {
            GCSkillFailed1 _GCSkillFailed1;
            _GCSkillFailed1.setSkillType(SKILL_THROW_HOLY_WATER);
            pPlayer->sendPacket(&_GCSkillFailed1);

            return;
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
            g_Sniping.checkRevealRatio(pCreature, 20, 10);
        }

        SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SKILL_THROW_HOLY_WATER);
        Assert(pSkillHandler != NULL);
        pSkillHandler->execute(pSlayer, pPacket->getTargetObjectID(), pPacket->getObjectID(), pPacket->getX(),
                               pPacket->getY());
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
