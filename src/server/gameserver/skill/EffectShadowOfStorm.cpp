//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectShadowOfStorm.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectShadowOfStorm.h"

#include "Effect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectShadowOfStorm::EffectShadowOfStorm(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
    m_Damage = 0;
}

void EffectShadowOfStorm::checkPosition() {
    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j) {
            int tx = m_X + i;
            int ty = m_Y + j;

            if (!isValidZoneCoord(m_pZone, tx, ty))
                continue;
            Tile& tile = m_pZone->getTile(tx, ty);
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

                    m_TargetPositions[pCreature->getObjectID()].x = tx;
                    m_TargetPositions[pCreature->getObjectID()].y = ty;
                }
            }
        }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShadowOfStorm::affect()

{
    Assert(m_pZone != NULL);

    // Get the creature that cast this effect.
    // It may have left the zone, so this can be NULL.
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);

    for (int i = -2; i <= 2; ++i)
        for (int j = -2; j <= 2; ++j) {
            int tx = m_X + i;
            int ty = m_Y + j;

            if (!isValidZoneCoord(m_pZone, tx, ty))
                continue;

            // Fetches the tile the effect covers.
            Tile& tile = m_pZone->getTile(tx, ty);

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
                    // Checks acid immunity.
                    if (pCastCreature != NULL &&
                        (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
                         !canHit(pCastCreature, pCreature, SKILL_SHADOW_OF_STORM))) {
                        continue;
                    }

                    // 2003.1.10 by Sequoia
                    // Safe zone check
                    if (!checkZoneLevelToHitTarget(pCreature))
                        continue;

                    if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                        GCModifyInformation gcAttackerMI;
                        GCModifyInformation gcDefenderMI;

                        Damage_t damage = m_Damage;
                        unordered_map<ObjectID_t, TPOINT>::iterator itr =
                            m_TargetPositions.find(pCreature->getObjectID());

                        if (itr == m_TargetPositions.end()) {
                            m_TargetPositions[pCreature->getObjectID()].x = pCreature->getX();
                            m_TargetPositions[pCreature->getObjectID()].y = pCreature->getY();
                        } else {
                            if (itr->second.x == pCreature->getX() && itr->second.y == pCreature->getY()) {
                            } else {
                                m_TargetPositions[pCreature->getObjectID()].x = pCreature->getX();
                                m_TargetPositions[pCreature->getObjectID()].y = pCreature->getY();
                                damage /= 2;
                            }
                        }

                        if (pCreature->isSlayer()) {
                            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                            ::setDamage(pSlayer, damage, pCastCreature, SKILL_SHADOW_OF_STORM, &gcDefenderMI,
                                        &gcAttackerMI, true, false);

                            Player* pPlayer = pSlayer->getPlayer();
                            Assert(pPlayer != NULL);
                            pPlayer->sendPacket(&gcDefenderMI);
                        } else if (pCreature->isVampire()) {
                            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                            ::setDamage(pVampire, damage, pCastCreature, SKILL_SHADOW_OF_STORM, &gcDefenderMI,
                                        &gcAttackerMI, true, false);

                            Player* pPlayer = pVampire->getPlayer();
                            Assert(pPlayer != NULL);
                            pPlayer->sendPacket(&gcDefenderMI);
                        } else if (pCreature->isMonster()) {
                            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                            ::setDamage(pMonster, damage, pCastCreature, SKILL_SHADOW_OF_STORM, NULL, &gcAttackerMI,
                                        true, false);
                        } else
                            continue; // Ousters and NPCs take no damage.

                        // Grants experience if the target died.
                        if (pCastCreature != NULL) {
                            if (pCreature->isDead() && pCastCreature->isOusters()) {
                                Ousters* pCastOusters = dynamic_cast<Ousters*>(pCastCreature);
                                Assert(pCastOusters != NULL);

                                int exp = computeCreatureExp(pCreature, 70, pCastOusters);
                                shareOustersExp(pCastOusters, exp, gcAttackerMI);
                            }
                        }

                        // Alignment computation

                        if (gcAttackerMI.getShortCount() != 0 || gcAttackerMI.getLongCount() != 0)
                            pCastCreature->getPlayer()->sendPacket(&gcAttackerMI);
                    }
                }
            }
        }

    setNextTime(10);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectShadowOfStorm::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectShadowOfStorm::toString() const {
    StringStream msg;

    msg << "EffectShadowOfStorm(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
