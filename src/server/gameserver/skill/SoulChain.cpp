//////////////////////////////////////////////////////////////////////////////
// Filename    : SoulChain.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "SoulChain.h"

#include "EffectSoulChain.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GDRLairManager.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "PCFinder.h"
#include "PKZoneInfoManager.h"
#include "ZoneInfoManager.h"
#include "war/WarSystem.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer self handler
//////////////////////////////////////////////////////////////////////////////
void SoulChain::execute(Slayer* pSlayer, const string& targetName, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);

        if (pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        // Any Slayer grand master can use this skill.
        // It belongs to no domain, so the highest-level domain is used as the current one.
        SkillDomainType_t DomainType = pSlayer->getHighestSkillDomain();

        ZoneCoord_t x = pSlayer->getX();
        ZoneCoord_t y = pSlayer->getY();

        bool bValidTarget = false;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTarget = pcFinder.getCreature_LOCKED(targetName);
        if (pTarget != NULL) {
            Zone* pTargetZone = pTarget->getZone();
            if (pTargetZone != NULL) {
                // Target checks
                // Must be the same race.
                bool bSlayer = pTarget->isSlayer();
                // Must be an ordinary user.
                bool bPLAYER = pTarget->getCompetence() == PLAYER;
                bool bMasterLair =
                    pTargetZone->isMasterLair() || GDRLairManager::Instance().isGDRLairZone(pTargetZone->getZoneID());
                // Can the paid service be used?
                bool bValidPay = pGamePlayer->loginPayPlay(pGamePlayer->getSocket()->getHost(), pGamePlayer->getID()) ||
                                 pGamePlayer->isFamilyFreePass() ||
                                 !de::gameContext().zoneInfos().getZoneInfo(pTargetZone->getZoneID())->isPayPlay();

                // Cannot follow into the field HQ, the outskirts, the event arena, the OX event, or Temerie.
                bool bValidZone = pTargetZone->getZoneID() != 2101 && pTargetZone->getZoneID() != 2102 &&
                                  pTargetZone->getZoneID() != 1005 && pTargetZone->getZoneID() != 1006 &&
                                  pTargetZone->getZoneID() != 1122 && pTargetZone->getZoneID() != 1131 &&
                                  pTargetZone->getZoneID() != 1132 && pTargetZone->getZoneID() != 1133 &&
                                  pTargetZone->getZoneID() != 1134 && !pTargetZone->isCastleZone() &&
                                  // Cannot follow into a castle or a master lair either.
                                  // Cannot follow into Adam's holy land either.
                                  (!de::gameContext().warSystem().hasActiveRaceWar() || !pTargetZone->isHolyLand()) &&
                                  !pTargetZone->isCastle() && !pTargetZone->isMasterLair() &&
                                  !de::gameContext().pkZoneInfos().isPKZone(pTargetZone->getZoneID()) &&
                                  // Cannot go into a dynamic zone either.
                                  !pTargetZone->isDynamicZone();

                bValidTarget = bSlayer && bPLAYER && !bMasterLair && bValidPay && bValidZone;
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)

        int RequiredMP = pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bEffected = pSlayer->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pSlayer->hasRelicItem() ||
                         pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && !bEffected && bValidTarget) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Attach an effect that blocks movement for 10 seconds.
            EffectSoulChain* pEffect = new EffectSoulChain(pSlayer);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(100);
            pEffect->setTargetName(targetName);
            pEffect->setZone(pZone);

            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            pZone->addEffect(pEffect);
            pSlayer->setFlag(Effect::EFFECT_CLASS_SOUL_CHAIN);

            // Raises experience.
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);

            int STRPoint, DEXPoint, INTPoint;
            switch (DomainType) {
            case SKILL_DOMAIN_BLADE:
            case SKILL_DOMAIN_SWORD:
                STRPoint = 8;
                DEXPoint = 1;
                INTPoint = 1;
                break;
            case SKILL_DOMAIN_GUN:
                STRPoint = 1;
                DEXPoint = 8;
                INTPoint = 1;
                break;
            case SKILL_DOMAIN_ENCHANT:
            case SKILL_DOMAIN_HEAL:
                STRPoint = 1;
                DEXPoint = 1;
                INTPoint = 8;
                break;
            default:
                Assert(false);
            }
            shareAttrExp(pSlayer, ExpUp, STRPoint, DEXPoint, INTPoint, _GCSkillToSelfOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToSelfOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToSelfOK1);

            // Send the packet.
            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);
            _GCSkillToSelfOK1.addShortData(MODIFY_EFFECT_STAT, Effect::EFFECT_CLASS_SOUL_CHAIN);

            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}


//////////////////////////////////////////////////////////////////////////////
// Vampire self
//////////////////////////////////////////////////////////////////////////////
void SoulChain::execute(Vampire* pVampire, const string& targetName, VampireSkillSlot* pSkillSlot,
                        CEffectID_t CEffectID)

{
    Assert(pVampire != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pVampire->getPlayer();
        Zone* pZone = pVampire->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);

        if (pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pVampire->getX();
        ZoneCoord_t y = pVampire->getY();

        bool bValidTarget = false;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTarget = pcFinder.getCreature_LOCKED(targetName);
        if (pTarget != NULL) {
            Zone* pTargetZone = pTarget->getZone();
            if (pTargetZone != NULL) {
                // Target checks
                // Must be the same race.
                bool bVampire = pTarget->isVampire();
                // Must be an ordinary user.
                bool bPLAYER = pTarget->getCompetence() == PLAYER;
                // Moving to a master lair is not allowed.
                bool bMasterLair =
                    pTargetZone->isMasterLair() || GDRLairManager::Instance().isGDRLairZone(pTargetZone->getZoneID());
                // Can the paid service be used?
                bool bValidPay = pGamePlayer->loginPayPlay(pGamePlayer->getSocket()->getHost(), pGamePlayer->getID()) ||
                                 pGamePlayer->isFamilyFreePass() ||
                                 !de::gameContext().zoneInfos().getZoneInfo(pTargetZone->getZoneID())->isPayPlay();

                // Cannot follow into the field HQ, the outskirts, the event arena, the OX event, or Temerie.
                bool bValidZone = pTargetZone->getZoneID() != 2101 && pTargetZone->getZoneID() != 2102 &&
                                  pTargetZone->getZoneID() != 1005 && pTargetZone->getZoneID() != 1006 &&
                                  pTargetZone->getZoneID() != 1122 && pTargetZone->getZoneID() != 1131 &&
                                  pTargetZone->getZoneID() != 1132 && pTargetZone->getZoneID() != 1133 &&
                                  pTargetZone->getZoneID() != 1134 && !pTargetZone->isCastleZone() &&
                                  (!de::gameContext().warSystem().hasActiveRaceWar() || !pTargetZone->isHolyLand()) &&
                                  !pTargetZone->isCastle() && !pTargetZone->isMasterLair() &&
                                  !de::gameContext().pkZoneInfos().isPKZone(pTargetZone->getZoneID()) &&
                                  // Cannot go into a dynamic zone either.
                                  !pTargetZone->isDynamicZone();

                bValidTarget = bVampire && bPLAYER && !bMasterLair && bValidPay && bValidZone;
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)

        int RequiredMP = decreaseConsumeMP(pVampire, pSkillInfo);
        bool bManaCheck = hasEnoughMana(pVampire, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bEffected = pVampire->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pVampire->hasRelicItem() ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pVampire->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && !bEffected && bValidTarget) {
            decreaseMana(pVampire, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pVampire);
            SkillOutput output;
            computeOutput(input, output);

            // Attach an effect that blocks movement for 10 seconds.
            EffectSoulChain* pEffect = new EffectSoulChain(pVampire);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(100);
            pEffect->setTargetName(targetName);
            pEffect->setZone(pZone);

            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            pZone->addEffect(pEffect);
            pVampire->setFlag(Effect::EFFECT_CLASS_SOUL_CHAIN);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);
            _GCSkillToSelfOK1.addShortData(MODIFY_EFFECT_STAT, Effect::EFFECT_CLASS_SOUL_CHAIN);

            _GCSkillToSelfOK2.setObjectID(pVampire->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2, pVampire);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pVampire, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pVampire, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////////////
// Ousters self
//////////////////////////////////////////////////////////////////////////////
void SoulChain::execute(Ousters* pOusters, const string& targetName, OustersSkillSlot* pSkillSlot,
                        CEffectID_t CEffectID)

{
    Assert(pOusters != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPlayer);
        Assert(pGamePlayer != NULL);

        if (pOusters->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) || pOusters->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER)) {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
            return;
        }

        GCSkillToSelfOK1 _GCSkillToSelfOK1;
        GCSkillToSelfOK2 _GCSkillToSelfOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);

        ZoneCoord_t x = pOusters->getX();
        ZoneCoord_t y = pOusters->getY();

        bool bValidTarget = false;
        PCFinder& pcFinder = de::gameContext().playerCreatures();

        __ENTER_CRITICAL_SECTION(pcFinder)

        Creature* pTarget = pcFinder.getCreature_LOCKED(targetName);
        if (pTarget != NULL) {
            Zone* pTargetZone = pTarget->getZone();
            if (pTargetZone != NULL) {
                // Target checks
                // Must be the same race.
                bool bOusters = pTarget->isOusters();
                // Must be an ordinary user.
                bool bPLAYER = pTarget->getCompetence() == PLAYER;
                // Moving to a master lair is not allowed.
                bool bMasterLair =
                    pTargetZone->isMasterLair() || GDRLairManager::Instance().isGDRLairZone(pTargetZone->getZoneID());
                // Can the paid service be used?
                bool bValidPay = pGamePlayer->loginPayPlay(pGamePlayer->getSocket()->getHost(), pGamePlayer->getID()) ||
                                 pGamePlayer->isFamilyFreePass() ||
                                 !de::gameContext().zoneInfos().getZoneInfo(pTargetZone->getZoneID())->isPayPlay();

                // Cannot follow into the field HQ, the outskirts, the event arena, the OX event, or Temerie.
                bool bValidZone = pTargetZone->getZoneID() != 2101 && pTargetZone->getZoneID() != 2102 &&
                                  pTargetZone->getZoneID() != 1005 && pTargetZone->getZoneID() != 1006 &&
                                  pTargetZone->getZoneID() != 1122 && pTargetZone->getZoneID() != 1131 &&
                                  pTargetZone->getZoneID() != 1132 && pTargetZone->getZoneID() != 1133 &&
                                  pTargetZone->getZoneID() != 1134 && !pTargetZone->isCastleZone() &&
                                  (!de::gameContext().warSystem().hasActiveRaceWar() || !pTargetZone->isHolyLand()) &&
                                  !pTargetZone->isCastle() && !pTargetZone->isMasterLair() &&
                                  !de::gameContext().pkZoneInfos().isPKZone(pTargetZone->getZoneID()) &&
                                  // Cannot go into a dynamic zone either.
                                  !pTargetZone->isDynamicZone();

                bValidTarget = bOusters && bPLAYER && !bMasterLair && bValidPay && bValidZone;
            }
        }

        __LEAVE_CRITICAL_SECTION(pcFinder)

        int RequiredMP = pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pOusters, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bEffected = pOusters->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pOusters->hasRelicItem() ||
                         pOusters->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pOusters->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && !bEffected && bValidTarget) {
            decreaseMana(pOusters, RequiredMP, _GCSkillToSelfOK1);

            SkillInput input(pOusters, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Attach an effect that blocks movement for 10 seconds.
            EffectSoulChain* pEffect = new EffectSoulChain(pOusters);
            pEffect->setDuration(output.Duration);
            pEffect->setDeadline(100);
            pEffect->setTargetName(targetName);
            pEffect->setZone(pZone);

            ObjectRegistry& objectregister = pZone->getObjectRegistry();
            objectregister.registerObject(pEffect);

            pZone->addEffect(pEffect);
            pOusters->setFlag(Effect::EFFECT_CLASS_SOUL_CHAIN);

            _GCSkillToSelfOK1.setSkillType(SkillType);
            _GCSkillToSelfOK1.setCEffectID(CEffectID);
            _GCSkillToSelfOK1.setDuration(0);
            _GCSkillToSelfOK1.addShortData(MODIFY_EFFECT_STAT, Effect::EFFECT_CLASS_SOUL_CHAIN);

            _GCSkillToSelfOK2.setObjectID(pOusters->getObjectID());
            _GCSkillToSelfOK2.setSkillType(SkillType);
            _GCSkillToSelfOK2.setDuration(0);

            // Send Packet
            pPlayer->sendPacket(&_GCSkillToSelfOK1);

            pZone->broadcastPacket(x, y, &_GCSkillToSelfOK2, pOusters);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pOusters, getSkillType(), NULL);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pOusters, getSkillType());
    }
}

SoulChain g_SoulChain;
