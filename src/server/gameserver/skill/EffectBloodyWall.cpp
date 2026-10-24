//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBloodyWall.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBloodyWall.h"

#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBloodyWall::EffectBloodyWall(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_CasterName = "";
    m_CasterID = 0;
    m_PartyID = 0;

    m_CreatureClass = Creature::CREATURE_CLASS_VAMPIRE;
    m_ClanID = 0;

    m_bForce = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::affect()

{
    Assert(m_pZone != NULL);

    // Fetches the creature that used the effect.
    // It may be NULL: the creature may already have left the zone.
    // by bezz. 2003.1.4
    Creature* pCastCreature = m_pZone->getCreature(m_CasterID);

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
            // The caster itself is not hit.
            // Safe zone check.
            // 2003.1.10 by bezz, Sequoia
            if (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
                pCreature->getObjectID() == m_CasterID || !checkZoneLevelToHitTarget(pCreature)) {
                continue;
            }

            // A creature of the same class is not hit.
            if (m_CreatureClass == pCreature->getCreatureClass() && !isForce()) {
                // Vampires do not hit each other.
                if (m_CreatureClass == Creature::CREATURE_CLASS_VAMPIRE) {
                    continue; // by sigi. 2003.1.14
                } else if (m_CreatureClass == Creature::CREATURE_CLASS_MONSTER) {
                    Creature* pAttacker = m_pZone->getCreature(m_CasterID);
                    if (pAttacker != NULL && pAttacker->isMonster()) {
                        Monster* pAttackMonster = dynamic_cast<Monster*>(pAttacker);
                        Monster* pDefendMonster = dynamic_cast<Monster*>(pCreature);

                        if (pAttackMonster->getClanType() == pDefendMonster->getClanType()) {
                            continue;
                        }
                    }
                }
            }

            int Damage = m_Damage;

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                    GCModifyInformation gcMI;
                    ::setDamage(pSlayer, Damage, pCastCreature, SKILL_BLOODY_WALL, &gcMI);

                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 50; // 50% chance of knockback
                    if (bKnockback) {
                        int x = pCreature->getX() + rand() % 3 - 1;
                        int y = pCreature->getY() + rand() % 3 - 1;
                        knockbackCreature(m_pZone, pCreature, x, y);
                        // This changes the Tile's oList, so stop checking here.
                        // Once one creature on a tile is knocked back, the rest can be skipped.
                        break;
                    }
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    GCModifyInformation gcMI;
                    ::setDamage(pVampire, Damage, pCastCreature, SKILL_BLOODY_WALL, &gcMI);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 50; // 50% chance of knockback
                    if (bKnockback) {
                        int x = pCreature->getX() + rand() % 3 - 1;
                        int y = pCreature->getY() + rand() % 3 - 1;
                        knockbackCreature(m_pZone, pCreature, x, y);
                        // This changes the Tile's oList, so stop checking here.
                        // Once one creature on a tile is knocked back, the rest can be skipped.
                        break;
                    }
                } else if (pCreature->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    GCModifyInformation gcMI;
                    ::setDamage(pOusters, Damage, pCastCreature, SKILL_BLOODY_WALL, &gcMI);

                    Player* pPlayer = pOusters->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 50; // 50% chance of knockback
                    if (bKnockback) {
                        int x = pCreature->getX() + rand() % 3 - 1;
                        int y = pCreature->getY() + rand() % 3 - 1;
                        knockbackCreature(m_pZone, pCreature, x, y);
                        // This changes the Tile's oList, so stop checking here.
                        // Once one creature on a tile is knocked back, the rest can be skipped.
                        break;
                    }
                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, Damage, pCastCreature, SKILL_BLOODY_WALL);

                    if (pCastCreature != NULL && pCastCreature->isPC()) {
                        pMonster->addEnemy(pCastCreature);
                    }

                    // Knockback check
                    bool bKnockback = rand() % 100 < 50; // 50% chance of knockback
                    if (bKnockback) {
                        int x = pCreature->getX() + rand() % 3 - 1;
                        int y = pCreature->getY() + rand() % 3 - 1;
                        knockbackCreature(m_pZone, pCreature, x, y);
                        // This changes the Tile's oList, so stop checking here.
                        // Once one creature on a tile is knocked back, the rest can be skipped.
                        break;
                    }
                }

                // Raises experience if the target died.
                if (pCreature->isDead()) {
                    if (pCastCreature != NULL && pCastCreature->isVampire()) {
                        Vampire* pVampire = dynamic_cast<Vampire*>(pCastCreature);
                        Assert(pVampire != NULL);

                        GCModifyInformation gcAttackerMI;
                        int exp = computeCreatureExp(pCreature, KILL_EXP);
                        shareVampExp(pVampire, exp, gcAttackerMI);

                        pVampire->getPlayer()->sendPacket(&gcAttackerMI);
                    }
                }

                // KillCount handling for when m_CasterName kills pCreature.
                // by sigi. 2002.8.31
                // Handled by the setDamage call.
                // by bezz. 2003.1.3
            }
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodyWall::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBloodyWall::toString() const {
    StringStream msg;

    msg << "EffectBloodyWall(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
