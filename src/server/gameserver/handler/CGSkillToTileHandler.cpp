//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToTileHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSkillToTile.h"

#ifdef __GAME_SERVER__
#include "Creature.h"
#include "GCSkillFailed1.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "SkillHandlerManager.h"
#include "ZoneUtil.h"
#include "skill/EffectAberration.h"
#include "skill/Sniping.h"

// #define __PROFILE_SKILLS__

#ifdef __PROFILE_SKILLS__
#include "Profile.h"
#endif
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSkillToTileHandler::execute(CGSkillToTile* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SkillHandlerManager& skillHandlers = de::gameContext().skillHandlers();

    try {
        // Code put in temporarily to test the special skills.
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);

        Assert(pGamePlayer != NULL); // by sigi

        if (pGamePlayer->getPlayerStatus() == GPS_NORMAL) {
            Creature* pCreature = pGamePlayer->getCreature();
            Assert(pCreature != NULL); // by sigi

            Zone* pZone = pCreature->getZone();
            Assert(pZone != NULL);

            SkillType_t SkillType = pPacket->getSkillType();

            // A complete safe zone forbids skill use.
            ZoneLevel_t ZoneLevel = pZone->getZoneLevel(pCreature->getX(), pCreature->getY());
            if ((ZoneLevel & COMPLETE_SAFE_ZONE) || (pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE)) ||
                (pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS)) ||
                (pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER)) ||
                (pCreature->isFlag(Effect::EFFECT_CLASS_COMA))) {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);

                return;
            }
            if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
                switch (SkillType) {
                case SKILL_ATTACK_MELEE:
                case SKILL_BITE_OF_DEATH:
                case SKILL_UN_TRANSFORM:
                case SKILL_RAPID_GLIDING:
                    break;
                default:
                    GCSkillFailed1 _GCSkillFailed1;
                    _GCSkillFailed1.setSkillType(SkillType);
                    pPlayer->sendPacket(&_GCSkillFailed1);
                    dynamic_cast<Vampire*>(pCreature)->sendVampireSkillInfo();
                    return;
                }
            }
            if (pCreature->isFlag(Effect::EFFECT_CLASS_ABERRATION)) {
                EffectAberration* pEffect =
                    dynamic_cast<EffectAberration*>(pCreature->findEffect(Effect::EFFECT_CLASS_ABERRATION));
                if (pEffect != NULL && (rand() % 100) < pEffect->getRatio()) {
                    Dir_t dir = rand() % 8;
                    pPacket->setX(pPacket->getX() + dirMoveMask[dir].x * 3);
                    pPacket->setY(pPacket->getY() + dirMoveMask[dir].y * 3);
                }
            }

            disableFlags(pCreature, pZone, SkillType);

            if (pCreature->isSlayer()) {
                Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
                SkillSlot* pSkillSlot = ((Slayer*)pCreature)->hasSkill(SkillType);
                bool bSuccess = true;

                if (SkillType == SKILL_BURNING_SOL_LAUNCH) {
                    pSkillSlot = ((Slayer*)pCreature)->hasSkill(SKILL_BURNING_SOL_CHARGING);
                }

                if (SkillType == SKILL_SWEEP_VICE_3 || SkillType == SKILL_SWEEP_VICE_5) {
                    pSkillSlot = ((Slayer*)pCreature)->hasSkill(SKILL_SWEEP_VICE_1);
                }

                if (SkillType == SKILL_TURRET_FIRE)
                    pSkillSlot = ((Slayer*)pCreature)->hasSkill(SKILL_INSTALL_TURRET);

                if (pSkillSlot == NULL)
                    bSuccess = false;
                if (!isAbleToUseTileSkill(pSlayer, SkillType))
                    bSuccess = false;


                if (bSuccess) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                    Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                    beginProfileEx(SkillTypes2String[SkillType]);
                    pSkillHandler->execute(pSlayer, pPacket->getX(), pPacket->getY(), pSkillSlot,
                                           pPacket->getCEffectID());
                    endProfileEx(SkillTypes2String[SkillType]);
#else
                    pSkillHandler->execute(pSlayer, pPacket->getX(), pPacket->getY(), pSkillSlot,
                                           pPacket->getCEffectID());
#endif
                } else {
                    GCSkillFailed1 _GCSkillFailed1;
                    _GCSkillFailed1.setSkillType(SkillType);
                    pPlayer->sendPacket(&_GCSkillFailed1);
                }
            } else if (pCreature->isVampire()) {
                Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
                VampireSkillSlot* pVampireSkillSlot = pVampire->hasSkill(SkillType);
                bool bSuccess = true;

                if (SkillType == SKILL_EAT_CORPSE && pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SKILL_EAT_CORPSE);
                    Assert(pSkillHandler != NULL);
                    pSkillHandler->execute(pVampire, pPacket->getX(), pPacket->getY(), pVampireSkillSlot,
                                           pPacket->getCEffectID());
                    return;
                }

                if (pVampireSkillSlot == NULL)
                    bSuccess = false;
                if (!isAbleToUseTileSkill(pVampire, SkillType))
                    bSuccess = false;

                if (bSuccess) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                    Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                    beginProfileEx(SkillTypes2String[SkillType]);
                    pSkillHandler->execute(pVampire, pPacket->getX(), pPacket->getY(), pVampireSkillSlot,
                                           pPacket->getCEffectID());
                    endProfileEx(SkillTypes2String[SkillType]);
#else
                    pSkillHandler->execute(pVampire, pPacket->getX(), pPacket->getY(), pVampireSkillSlot,
                                           pPacket->getCEffectID());
#endif
                } else {
                    GCSkillFailed1 _GCSkillFailed1;
                    _GCSkillFailed1.setSkillType(SkillType);
                    pPlayer->sendPacket(&_GCSkillFailed1);
                }
            } else if (pCreature->isOusters()) {
                Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
                OustersSkillSlot* pOustersSkillSlot = pOusters->hasSkill(SkillType);
                bool bSuccess = true;

                if (pOustersSkillSlot == NULL)
                    bSuccess = false;
                if (!isAbleToUseTileSkill(pOusters, SkillType))
                    bSuccess = false;
                if (SkillType == SKILL_DESTRUCTION_SPEAR &&
                    pOusters->hasSkill(SKILL_DESTRUCTION_SPEAR_MASTERY) == NULL) {
                    bSuccess = false;
                }
                if (SkillType == SKILL_ICE_LANCE && pOusters->hasSkill(SKILL_ICE_LANCE_MASTERY) == NULL) {
                    bSuccess = false;
                }

                if (bSuccess) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                    Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                    beginProfileEx(SkillTypes2String[SkillType]);
                    pSkillHandler->execute(pOusters, pPacket->getX(), pPacket->getY(), pOustersSkillSlot,
                                           pPacket->getCEffectID());
                    endProfileEx(SkillTypes2String[SkillType]);
#else
                    pSkillHandler->execute(pOusters, pPacket->getX(), pPacket->getY(), pOustersSkillSlot,
                                           pPacket->getCEffectID());
#endif
                } else {
                    GCSkillFailed1 _GCSkillFailed1;
                    _GCSkillFailed1.setSkillType(SkillType);
                    pPlayer->sendPacket(&_GCSkillFailed1);
                }
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
