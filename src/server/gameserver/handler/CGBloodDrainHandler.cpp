//////////////////////////////////////////////////////////////////////////////
// Filename    : CGBloodDrainHandler.cpp
// Written By  : crazydog
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGBloodDrain.h"

#ifdef __GAME_SERVER__
#include "GCAttack.h"
#include "GCGetDamage.h"
#include "GCSkillFailed1.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Skill.h"
#include "SkillHandlerManager.h"
#include "Vampire.h"
#include "ZonePlayerManager.h"
#include "ZoneUtil.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGBloodDrainHandler::execute(CGBloodDrain* pPacket, Player* pPlayer)

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

            // Only a Vampire can drain blood.
            if (!pCreature->isVampire())
                return;

            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            // A complete safe zone forbids skill use.
            ZoneLevel_t ZoneLevel = pCreature->getZone()->getZoneLevel(pCreature->getX(), pCreature->getY());
            if ((ZoneLevel & COMPLETE_SAFE_ZONE) || (!isAbleToUseObjectSkill(pVampire, SKILL_BLOOD_DRAIN))) {
                GCSkillFailed1 gcSkillFailed1;
                gcSkillFailed1.setSkillType(SKILL_BLOOD_DRAIN);
                pPlayer->sendPacket(&gcSkillFailed1);
                return;
            }

            // A blood drain attempted in the Dark Revenge state clears that state.
            if (pVampire->isFlag(Effect::EFFECT_CLASS_EXTREME)) {
                EffectManager* pEffectManager = pVampire->getEffectManager();
                Assert(pEffectManager != NULL);
                Effect* pEffect = pEffectManager->findEffect(Effect::EFFECT_CLASS_EXTREME);
                if (pEffect != NULL) {
                    pEffect->setDeadline(0);
                }
            }

            // A blood drain attempted while invisible makes one visible again.
            if (pVampire->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
                Zone* pZone = pVampire->getZone();
                Assert(pZone != NULL);
                addVisibleCreature(pZone, pVampire, true);
            }

            SkillHandler* pSkillHandler = de::gameContext().skillHandlers().getSkillHandler(SKILL_BLOOD_DRAIN);
            Assert(pSkillHandler != NULL);
            pSkillHandler->execute(pVampire, pPacket->getObjectID());
        }
    } catch (Throwable& t) {
    }

#endif

    __END_DEBUG
    __END_DEBUG_EX
}
