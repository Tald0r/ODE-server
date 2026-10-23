//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectSharpHail.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectSharpHail.h"

#include "DB.h"
#include "Effect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "HitRoll.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectSharpHail::EffectSharpHail(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
    m_bForce = false;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSharpHail::affect()

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
            if (pCastCreature != NULL &&
                (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
                 !canHit(pCastCreature, pCreature, SKILL_SHARP_HAIL, getLevel()))) {
                continue;
            }

            // 2003.1.10 by Sequoia
            // Safe zone check.
            if (!checkZoneLevelToHitTarget(pCreature))
                continue;
            if (pCastCreature != NULL && !HitRoll::isSuccess(pCastCreature, pCreature))
                continue;

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                GCModifyInformation gcAttackerMI;
                GCModifyInformation gcDefenderMI;

                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                    ::setDamage(pSlayer, m_Damage, pCastCreature, SKILL_SHARP_HAIL, &gcDefenderMI, &gcAttackerMI, true,
                                true);

                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    ::setDamage(pVampire, m_Damage, pCastCreature, SKILL_SHARP_HAIL, &gcDefenderMI, &gcAttackerMI, true,
                                true);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, m_Damage, pCastCreature, SKILL_SHARP_HAIL, NULL, &gcAttackerMI, true, true);
                } else if (pCreature->isOusters() && isForce()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    ::setDamage(pOusters, m_Damage, pCastCreature, SKILL_SHARP_HAIL, &gcDefenderMI, &gcAttackerMI, true,
                                true);

                    Player* pPlayer = pOusters->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else
                    continue; // Skips NPCs and Ousters outside a forced cast.

                // Grant experience if the target died.
                if (pCastCreature != NULL) {
                    if (pCreature->isDead() && pCastCreature->isOusters()) {
                        Ousters* pCastOusters = dynamic_cast<Ousters*>(pCastCreature);
                        Assert(pCastOusters != NULL);

                        int exp = computeCreatureExp(pCreature, 100, pCastOusters);
                        shareOustersExp(pCastOusters, exp, gcAttackerMI);
                    }
                }

                if (gcAttackerMI.getShortCount() != 0 || gcAttackerMI.getLongCount() != 0)
                    pCastCreature->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectSharpHail::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectSharpHail::toString() const {
    StringStream msg;

    msg << "EffectSharpHail(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
