//////////////////////////////////////////////////////////////////////////////
// Filename    : CGCastingSkillHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGCastingSkill.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCCastingSkill.h"
#include "GamePlayer.h"
#include "SkillHandlerManager.h"
#include "item/Motorcycle.h"
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGCastingSkillHandler::execute(CGCastingSkill* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

        Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
            // Get the Creature.
            Creature* pCreature = pGamePlayer->getCreature();

            Assert(pCreature != NULL);

            GCCastingSkill _GCCastingSkill;
            _GCCastingSkill.setSkillType(pPacket->getSkillType());
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
