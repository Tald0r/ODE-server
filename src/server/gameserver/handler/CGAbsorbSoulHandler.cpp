//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAbsorbSoulHandler.cpp
// Written By  : crazydog
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGAbsorbSoul.h"

#ifdef __GAME_SERVER__
#include "GCAttack.h"
#include "GCGetDamage.h"
#include "GCSkillFailed1.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Ousters.h"
#include "Skill.h"
#include "SkillHandlerManager.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGAbsorbSoulHandler::execute(CGAbsorbSoul* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX __BEGIN_DEBUG

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);

        if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
            Creature* pCreature = pGamePlayer->getCreature();
            Assert(pCreature != NULL);

            // Only an Ousters can absorb soul.
            if (!pCreature->isOusters())
                return;

            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            // A complete safe zone forbids skill use.
            ZoneLevel_t ZoneLevel = pCreature->getZone()->getZoneLevel(pCreature->getX(), pCreature->getY());
            if ((ZoneLevel & COMPLETE_SAFE_ZONE) || (!isAbleToUseObjectSkill(pOusters, SKILL_ABSORB_SOUL))) {
                GCSkillFailed1 gcSkillFailed1;
                gcSkillFailed1.setSkillType(SKILL_ABSORB_SOUL);
                pPlayer->sendPacket(&gcSkillFailed1);
                return;
            }

            SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SKILL_ABSORB_SOUL);
            Assert(pSkillHandler != NULL);
            pSkillHandler->execute(pOusters, pPacket->getObjectID(), pPacket->getTargetZoneX(),
                                   pPacket->getTargetZoneY(), pPacket->getInvenObjectID(), pPacket->getInvenX(),
                                   pPacket->getInvenY(), pPacket->getTargetInvenX(), pPacket->getTargetInvenY());
        }
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
