//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToSelfHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSkillToSelf.h"

#ifdef __GAME_SERVER__
#include "GCSkillFailed1.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "RelicUtil.h"
#include "SkillHandlerManager.h"
#include "ZoneUtil.h"
#include "skill/Sniping.h"

// #define __PROFILE_SKILLS__

#ifdef __PROFILE_SKILLS__
#include "Profile.h"
#endif
#endif // __GAME_SERVER__

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CGSkillToSelfHandler::execute(CGSkillToSelf* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SkillHandlerManager& skillHandlers = de::gameContext().skillHandlers();

    try {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL); // by sigi

        if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
            return;

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
        disableFlags(pCreature, pZone, SkillType);

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            SkillSlot* pSkillSlot = ((Slayer*)pCreature)->hasSkill(SkillType);
            bool bSuccess = true;

            if (pSkillSlot == NULL)
                bSuccess = false;
            if (!isAbleToUseSelfSkill(pSlayer, SkillType))
                bSuccess = false;


            // For UN_TRANSFORM, a handler of its own does the work
            if (SkillType == SKILL_UN_TRANSFORM) {
                if (pSlayer->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SKILL_UN_TRANSFORM);
                    Assert(pSkillHandler != NULL);
                    pSkillHandler->execute(pSlayer, NULL, (CEffectID_t)0);
                    return;
                }
            }

            if (bSuccess) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                pSkillHandler->execute(pSlayer, pSkillSlot, pPacket->getCEffectID());
                endProfileEx(SkillTypes2String[SkillType]);
#else
                pSkillHandler->execute(pSlayer, pSkillSlot, pPacket->getCEffectID());
#endif
            } else {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);
            }
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
            VampireSkillSlot* pVampireSkillSlot = ((Vampire*)pCreature)->hasSkill(SkillType);
            bool bSuccess = true;

            if ((SkillType == SKILL_TRANSFORM_TO_BAT || SkillType == SKILL_TRANSFORM_TO_WOLF) &&
                (pVampire->hasRelicItem() || pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                 pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))) {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);
                return;
            }

            if (SkillType == SKILL_UN_INVISIBILITY && pVampire->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
                addVisibleCreature(pZone, pVampire, true);
                return;
            }
            if (SkillType == SKILL_UN_TRANSFORM) {
                if (pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
                    pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
                    pVampire->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SKILL_UN_TRANSFORM);
                    Assert(pSkillHandler != NULL);
                    pSkillHandler->execute(pVampire);
                    return;
                }
            }
            if (SkillType == SKILL_OPEN_CASKET && pVampire->isFlag(Effect::EFFECT_CLASS_CASKET)) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SKILL_OPEN_CASKET);
                Assert(pSkillHandler != NULL);
                pSkillHandler->execute(pVampire, pVampireSkillSlot, pPacket->getCEffectID());
                return;
            }


            if (pVampireSkillSlot == NULL)
                bSuccess = false;
            if (!isAbleToUseSelfSkill(pVampire, SkillType))
                bSuccess = false;


            if (bSuccess) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                pSkillHandler->execute(pVampire, pVampireSkillSlot, pPacket->getCEffectID());
                endProfileEx(SkillTypes2String[SkillType]);
#else
                pSkillHandler->execute(pVampire, pVampireSkillSlot, pPacket->getCEffectID());
#endif
            } else {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            OustersSkillSlot* pOustersSkillSlot = ((Ousters*)pCreature)->hasSkill(SkillType);
            bool bSuccess = true;

            // For UN_TRANSFORM, a handler of its own does the work
            if (SkillType == SKILL_UN_TRANSFORM) {
                if (pOusters->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
                    SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SKILL_UN_TRANSFORM);
                    Assert(pSkillHandler != NULL);
                    pSkillHandler->execute(pOusters);
                    return;
                }
            }

            if (pOustersSkillSlot == NULL)
                bSuccess = false;
            if (!isAbleToUseSelfSkill(pOusters, SkillType))
                bSuccess = false;

            if (bSuccess) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                pSkillHandler->execute(pOusters, pOustersSkillSlot, pPacket->getCEffectID());
                endProfileEx(SkillTypes2String[SkillType]);
#else
                pSkillHandler->execute(pOusters, pOustersSkillSlot, pPacket->getCEffectID());
#endif
            } else {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);
            }
        }
    } catch (Throwable& t) {
    }

#endif // __GAME_SERVER__

    __END_DEBUG_EX
}
