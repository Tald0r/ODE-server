//////////////////////////////////////////////////////////////////////////////
// Filename    : CGSkillToObjectHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CGSkillToObject.h"

#ifdef __GAME_SERVER__
#include "Effect.h"
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
void CGSkillToObjectHandler::execute(CGSkillToObject* pPacket, Player* pPlayer)

{
    __BEGIN_DEBUG_EX

#ifdef __GAME_SERVER__

    Assert(pPacket != NULL);
    Assert(pPlayer != NULL);

    SkillHandlerManager& skillHandlers = de::gameContext().skillHandlers();

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
    Assert(pGamePlayer != NULL);

    if (pGamePlayer->getPlayerStatus() != GPS_NORMAL)
        return;

    try {
        Creature* pCreature = pGamePlayer->getCreature();
        Assert(pCreature != NULL);

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

        ObjectID_t TargetObjectID = pPacket->getTargetObjectID();
        CEffectID_t EffectID = pPacket->getCEffectID();

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);
        if (pTargetCreature != NULL && pTargetCreature->getCreatureClass() != pCreature->getCreatureClass()) {
            pCreature->setLastTarget(TargetObjectID);
        }

        if (pCreature->isFlag(Effect::EFFECT_CLASS_ABERRATION)) {
            EffectAberration* pEffect =
                dynamic_cast<EffectAberration*>(pCreature->findEffect(Effect::EFFECT_CLASS_ABERRATION));
            if (pEffect != NULL && (rand() % 100) < pEffect->getRatio()) {
                GCSkillFailed1 _GCSkillFailed1;
                _GCSkillFailed1.setSkillType(SkillType);
                pPlayer->sendPacket(&_GCSkillFailed1);

                return;
            }
        }

        disableFlags(pCreature, pZone, SkillType);

        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            SkillSlot* pSkillSlot = ((Slayer*)pCreature)->hasSkill(SkillType);
            bool bSuccess = true;
            if (SkillType == SKILL_TURRET_FIRE)
                pSkillSlot = ((Slayer*)pCreature)->hasSkill(SKILL_INSTALL_TURRET);

            if (pSkillSlot == NULL)
                bSuccess = false;
            if (!isAbleToUseObjectSkill(pSlayer, SkillType))
                bSuccess = false;

            // A relic table of one's own race does not accept the skill.
            if (bSuccess) {
                SkillHandler* SkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(SkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                SkillHandler->execute(pSlayer, TargetObjectID, pSkillSlot, EffectID);
                endProfileEx(SkillTypes2String[SkillType]);
#else
                SkillHandler->execute(pSlayer, TargetObjectID, pSkillSlot, EffectID);
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

            // The BITE OF DEATH werewolf check happens inside isAbleToUseObjectSkill.
            if (pVampireSkillSlot == NULL && SkillType != SKILL_BITE_OF_DEATH)
                bSuccess = false;
            if (!isAbleToUseObjectSkill(pVampire, SkillType))
                bSuccess = false;


            // An EXTREME skill in use has to be released.
            // The EXTREME skill cannot be used alongside another skill.
            // The reason is that EXTREME raises the damage far too much, and
            // that using another skill while EXTREME is up leaves no Effect.

            // 2002.4.1
            // It was changed so that a MELEE Attack can be used while EXTREME is up.
            // because bare-handed attacks alone were felt to be too weak
            // The MELEE skills usable while EXTREME is up are
            //  ACID TOUCH, POISONOUS HAND and BLOODY NAIL.
            //


            if (bSuccess) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                pSkillHandler->execute(pVampire, TargetObjectID, pVampireSkillSlot, EffectID);
                endProfileEx(SkillTypes2String[SkillType]);
#else
                pSkillHandler->execute(pVampire, TargetObjectID, pVampireSkillSlot, EffectID);
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
            if (!isAbleToUseObjectSkill(pOusters, SkillType))
                bSuccess = false;

            if (bSuccess) {
                SkillHandler* pSkillHandler = skillHandlers.getSkillHandler(SkillType);
                Assert(pSkillHandler != NULL);

#ifdef __PROFILE_SKILLS__
                beginProfileEx(SkillTypes2String[SkillType]);
                pSkillHandler->execute(pOusters, TargetObjectID, pOustersSkillSlot, EffectID);
                endProfileEx(SkillTypes2String[SkillType]);
#else
                pSkillHandler->execute(pOusters, TargetObjectID, pOustersSkillSlot, EffectID);
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
