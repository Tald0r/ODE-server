//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBloodySnake.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBloodySnake.h"

#include "GCAddEffectToTile.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

POINT EffectBloodySnake::getNextPosition() {
    Dir_t dir = m_Dir;
    if (rand() % 5 < 2) {
        dir = (dir + (rand() % 2 == 0 ? 1 : DIR_MAX - 1)) % DIR_MAX;
    }

    const POINT& pt = dirMoveMask[dir];

    return POINT(m_X + pt.x, m_Y + pt.y);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBloodySnake::EffectBloodySnake(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_Dir = DOWN;
    m_CasterName = "";
    m_CasterID = 0;

    m_CreatureClass = Creature::CREATURE_CLASS_VAMPIRE;
    m_ClanID = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::affect()

{
    Assert(m_pZone != NULL);

    // Get the skill user.
    // It can be NULL, since the creature may already have left the zone.
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
            if (!canAttack(pCastCreature, pCreature) || pCreature->getObjectID() == m_CasterID) {
                continue;
            }

            // A creature of the same class is not hit.
            if (m_CreatureClass == pCreature->getCreatureClass()) {
                if (m_CreatureClass == Creature::CREATURE_CLASS_MONSTER) {
                    Creature* pAttacker = m_pZone->getCreature(m_CasterID);
                    if (pAttacker != NULL) {
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
                    ::setDamage(pSlayer, Damage, pCastCreature, SKILL_BLOODY_SNAKE, &gcMI);

                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 20; // 20% chance of knockback
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
                    ::setDamage(pVampire, Damage, pCastCreature, SKILL_BLOODY_SNAKE, &gcMI);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 20; // 20% chance of knockback
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
                    ::setDamage(pOusters, Damage, pCastCreature, SKILL_BLOODY_SNAKE, &gcMI);

                    Player* pPlayer = pOusters->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcMI);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 20; // 20% chance of knockback
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

                    ::setDamage(pMonster, Damage, pCastCreature, SKILL_BLOODY_SNAKE);

                    // Knockback check
                    bool bKnockback = rand() % 100 < 20; // 20% chance of knockback
                    if (bKnockback) {
                        int x = pCreature->getX() + rand() % 3 - 1;
                        int y = pCreature->getY() + rand() % 3 - 1;
                        knockbackCreature(m_pZone, pCreature, x, y);
                        // This changes the Tile's oList, so stop checking here.
                        // Once one creature on a tile is knocked back, the rest can be skipped.
                        break;
                    }
                }


                // KillCount handling for when m_CasterName kills pCreature.
                // by sigi. 2002.8.31
            }
        }
    }

    // Compute the next move coordinates.
    POINT pt = getNextPosition();


    // Add it to the tile it moves to next.
    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);
    if (rect.ptInRect(pt.x, pt.y)) {
        Tile& newTile = m_pZone->getTile(pt.x, pt.y);
        if ((!newTile.isGroundBlocked() || newTile.hasCreature(Creature::MOVE_MODE_WALKING)) &&
            newTile.canAddEffect()) {
            // Delete the same effect if one is already there.
            Effect* pOldEffect = newTile.getEffect(Effect::EFFECT_CLASS_BLOODY_SNAKE);
            if (pOldEffect != NULL && pOldEffect != this) {
                ObjectID_t effectID = pOldEffect->getObjectID();
                m_pZone->deleteEffect(effectID); // fix me
            }

            // Remove it from the old tile,
            tile.deleteEffect(m_ObjectID);

            // and add it to the new tile.
            newTile.addEffect(this);

            m_X = pt.x;
            m_Y = pt.y;

            GCAddEffectToTile gcAddEffectToTile;
            gcAddEffectToTile.setEffectID(Effect::EFFECT_CLASS_BLOODY_SNAKE);
            gcAddEffectToTile.setDuration(m_Tick + (m_Tick >> 1));
            gcAddEffectToTile.setObjectID(m_ObjectID);
            gcAddEffectToTile.setXY(m_X, m_Y);

            m_pZone->broadcastPacket(m_X, m_Y, &gcAddEffectToTile);

            setNextTime(m_Tick);
        } else {
            setDeadline(0);
        }
    } else {
        setDeadline(0);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodySnake::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBloodySnake::toString() const {
    StringStream msg;

    msg << "EffectBloodySnake(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
