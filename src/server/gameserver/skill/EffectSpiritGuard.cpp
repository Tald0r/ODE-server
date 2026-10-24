//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSpiritGuard.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectSpiritGuard.h"

#include "GCAddEffectToTile.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "PCFinder.h"
#include "SkillInfo.h"
#include "SkillSlot.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSpiritGuard::EffectSpiritGuard(Creature* pCreature)

{
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSpiritGuard::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget); // by Sequoia

    if (pCreature != NULL) {
        affect(pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSpiritGuard::affect(Creature* pCastCreature)

{
    Assert(pCastCreature != NULL);

    if (!pCastCreature->isSlayer())
        return;

    Player* pPlayer = dynamic_cast<Player*>(pCastCreature->getPlayer());
    Assert(pPlayer != NULL);

    Slayer* pSlayer = dynamic_cast<Slayer*>(pCastCreature);
    Assert(pSlayer != NULL);

    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_SPIRIT_GUARD);
    if (pSkillInfo == NULL) {
        return;
    }

    GCModifyInformation gcAttackerMI;

    Zone* pZone = pCastCreature->getZone();
    Assert(pZone != NULL);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    ZoneCoord_t Cx = pCastCreature->getX();
    ZoneCoord_t Cy = pCastCreature->getY();

    bool isHit = false;

    Level_t maxEnemyLevel = 0;
    uint EnemyNum = 0;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            if (x == 0 && y == 0)
                continue;

            int X = Cx + x;
            int Y = Cy + y;

            if (!rect.ptInRect(X, Y))
                continue;

            // Fetches the objects on the tile.
            Tile& tile = pZone->getTile(X, Y);

            if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                Creature* pCreature = tile.getCreature(Creature::MOVE_MODE_WALKING);
                Assert(pCreature != NULL);

                // The target itself, invulnerable creatures, Slayers and NPCs are not hit.
                // Safe zone check
                // 2003.1.10 by bezz, Sequoia
                if (pCreature == m_pTarget || !canAttack(pCastCreature, pCreature) ||
                    pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isSlayer() || pCreature->isNPC() ||
                    !checkZoneLevelToHitTarget(pCreature)) {
                    continue;
                }

                isHit = true;

                if (maxEnemyLevel < pCreature->getLevel())
                    maxEnemyLevel = pCreature->getLevel();
                EnemyNum++;

                if (pCreature->isVampire() || pCreature->isOusters()) {
                    GCModifyInformation gcMI;
                    ::setDamage(pCreature, m_Damage, pCastCreature, SKILL_SPIRIT_GUARD, &gcMI, &gcAttackerMI);

                    pCreature->getPlayer()->sendPacket(&gcMI);

                    // Shows the hit animation.
                    GCSkillToObjectOK2 gcSkillToObjectOK2;
                    gcSkillToObjectOK2.setObjectID(1); // Unused
                    gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                    gcSkillToObjectOK2.setDuration(0);
                    pCreature->getPlayer()->sendPacket(&gcSkillToObjectOK2);

                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, m_Damage, pCastCreature, SKILL_SPIRIT_GUARD, NULL, &gcAttackerMI);

                    pMonster->addEnemy(pCastCreature);
                } else
                    Assert(false);

                GCSkillToObjectOK4 gcSkillToObjectOK4;
                gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                gcSkillToObjectOK4.setTargetObjectID(pCreature->getObjectID());
                gcSkillToObjectOK4.setDuration(0);

                pZone->broadcastPacket(X, Y, &gcSkillToObjectOK4, pCreature);
            }
        }
    }

    if (isHit) {
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillSlot* pSkillSlot = pSlayer->getSkill(SKILL_SPIRIT_GUARD);

        if (pSkillSlot != NULL) {
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), gcAttackerMI, maxEnemyLevel, EnemyNum);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, gcAttackerMI);
        }
    }

    setNextTime(m_Delay);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSpiritGuard::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    if (!pCreature->isSlayer())
        return;

    Player* pPlayer = dynamic_cast<Player*>(pCreature->getPlayer());
    Assert(pPlayer != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    // Removes the effect and announces it.
    pCreature->removeFlag(Effect::EFFECT_CLASS_SPIRIT_GUARD_1);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(m_EffectClass);

    pPlayer->sendPacket(&gcRemoveEffect);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect, pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSpiritGuard::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget); // by Sequoia

    if (pCreature != NULL) {
        unaffect(pCreature);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSpiritGuard::setLevel(SkillLevel_t Level) {
    if (Level <= GRADE_ADEPT_LIMIT_LEVEL)
        m_EffectClass = EFFECT_CLASS_SPIRIT_GUARD_1;
    else if (Level <= GRADE_EXPERT_LIMIT_LEVEL)
        m_EffectClass = EFFECT_CLASS_SPIRIT_GUARD_2;
    else if (Level <= GRADE_MASTER_LIMIT_LEVEL)
        m_EffectClass = EFFECT_CLASS_SPIRIT_GUARD_3;
    else
        m_EffectClass = EFFECT_CLASS_SPIRIT_GUARD_4;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSpiritGuard::toString() const {
    StringStream msg;
    msg << "EffectSpiritGuard(" << "Damage:" << (int)m_Damage << ", EffectClass:" << (int)m_EffectClass << ")";
    return msg.toString();
}
