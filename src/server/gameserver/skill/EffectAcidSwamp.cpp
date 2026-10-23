//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectAcidSwamp.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectAcidSwamp.h"

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectAcidSwamp::EffectAcidSwamp(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
    m_bVampire = false;
    m_bForce = false;

    m_TargetObjectID[0] = 0;
    m_TargetObjectID[1] = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::affect()

{
    Assert(m_pZone != NULL);

    // Get the creature that cast this effect.
    // It may have left the zone, so this can be NULL.
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);

    if (pCastCreature == NULL && !isForce()) {
        setNextTime(m_Tick);

        return;
    }

    // Get the tile this effect is attached to.
    Tile& tile = m_pZone->getTile(m_X, m_Y);

    // Walk the objects on the tile.
    const forward_list<Object*>& oList = tile.getObjectList();
    forward_list<Object*>::const_iterator itr = oList.begin();
    for (; itr != oList.end(); itr++) {
        Assert(*itr != NULL);

        Object* pObject = *itr;
        Assert(pObject != NULL);

        if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
            Creature* pCreature = dynamic_cast<Creature*>(pObject);
            Assert(pCreature != NULL);

            // Check for invulnerability.
            // Acid immunity.
            if (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_IMMUNE_TO_ACID) ||
                pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                continue;
            }

            // 2003.1.10 by Sequoia
            // Safe zone check.
            if (!checkZoneLevelToHitTarget(pCreature))
                continue;

            int AcidDamage = computeMagicDamage(pCreature, m_Damage, SKILL_ACID_SWAMP, m_bVampire, pCastCreature);

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                GCModifyInformation gcAttackerMI;
                GCModifyInformation gcDefenderMI;

                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                    ::setDamage(pSlayer, AcidDamage, pCastCreature, SKILL_ACID_SWAMP, &gcDefenderMI, &gcAttackerMI,
                                false);

                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    ::setDamage(pVampire, AcidDamage / 2, pCastCreature, SKILL_ACID_SWAMP, &gcDefenderMI, &gcAttackerMI,
                                false);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    ::setDamage(pOusters, AcidDamage, pCastCreature, SKILL_ACID_SWAMP, &gcDefenderMI, &gcAttackerMI,
                                false);

                    Player* pPlayer = pOusters->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, AcidDamage, pCastCreature, SKILL_ACID_SWAMP, NULL, &gcAttackerMI, false);
                }

                bool modifiedAttacker = false;

                // Grant experience if the target died.
                if (pCastCreature != NULL) {
                    if (pCreature->isDead() && pCastCreature->isVampire()) {
                        int exp = computeCreatureExp(pCreature, KILL_EXP);
                        Vampire* pCastVampire = dynamic_cast<Vampire*>(pCastCreature);
                        Assert(pCastVampire != NULL);

                        shareVampExp(pCastVampire, exp, gcAttackerMI);
                        modifiedAttacker = true;
                    }
                }

                // Compute the alignment change.
                if (pCastCreature != NULL && pCastCreature->isPC() && pCreature->isPC() &&
                    (pCreature->getObjectID() == m_TargetObjectID[0] ||
                     pCreature->getObjectID() == m_TargetObjectID[1])) {
                    computeAlignmentChange(pCreature, AcidDamage, pCastCreature, &gcDefenderMI, &gcAttackerMI);
                    modifiedAttacker = true;
                }

                if (modifiedAttacker)
                    pCastCreature->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectAcidSwamp::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectAcidSwamp::toString() const {
    StringStream msg;

    msg << "EffectAcidSwamp(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}

void EffectAcidSwampLoader::load(Zone* pZone)

{
    vector<ZoneEffectRow> rows =
        defaultZoneInfoRepository().loadZoneEffectRects(pZone->getZoneID(), (int)Effect::EFFECT_CLASS_ACID_SWAMP);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value2 = rows[r].value1; // the commented-out value1 read above shifted these
        int value3 = rows[r].value2;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (int X = left; X <= right; X++)
            for (int Y = top; Y <= bottom; Y++) {
                if (rect.ptInRect(X, Y)) {
                    Tile& tile = pZone->getTile(X, Y);
                    if (tile.canAddEffect()) {
                        EffectAcidSwamp* pEffect = new EffectAcidSwamp(pZone, X, Y);
                        pEffect->setForce(true);
                        pEffect->setTick(value2);
                        pEffect->setDamage(value3);
                        pEffect->setNextTime(0);
                        pEffect->setLevel(100);

                        // Register the effect in the zone and add it to the tile.
                        pZone->registerObject(pEffect);
                        tile.addEffect(pEffect);
                    }
                }
            }
    }
}
