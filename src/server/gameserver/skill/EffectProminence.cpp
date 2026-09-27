//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectProminence.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectProminence.h"

#include "DB.h"
#include "Effect.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "repository/ZoneInfoRepository.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectProminence::EffectProminence(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    __BEGIN_TRY

    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserObjectID = 0;
    m_SendEffectClass = getEffectClass();
    m_bForce = false;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectProminence::affect()

{
    __BEGIN_TRY


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
                 !canHit(pCastCreature, pCreature, SKILL_PROMINENCE, getLevel()))) {
                continue;
            }

            // 2003.1.10 by Sequoia
            // Safe zone check.
            if (!checkZoneLevelToHitTarget(pCreature))
                continue;

            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                GCModifyInformation gcAttackerMI;
                GCModifyInformation gcDefenderMI;

                if (pCreature->isSlayer()) {
                    Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                    ::setDamage(pSlayer, m_Damage, pCastCreature, SKILL_PROMINENCE, &gcDefenderMI, &gcAttackerMI, true,
                                false);

                    Player* pPlayer = pSlayer->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    ::setDamage(pVampire, m_Damage, pCastCreature, SKILL_PROMINENCE, &gcDefenderMI, &gcAttackerMI, true,
                                false);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, m_Damage, pCastCreature, SKILL_PROMINENCE, NULL, &gcAttackerMI, true, false);
                } else if (pCreature->isOusters() && isForce()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    ::setDamage(pOusters, m_Damage, pCastCreature, SKILL_PROMINENCE, &gcDefenderMI, &gcAttackerMI, true,
                                false);

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

                        int exp = computeCreatureExp(pCreature, 70, pCastOusters);
                        shareOustersExp(pCastOusters, exp, gcAttackerMI);
                    }
                }

                // Compute the alignment change.

                if (gcAttackerMI.getShortCount() != 0 || gcAttackerMI.getLongCount() != 0)
                    pCastCreature->getPlayer()->sendPacket(&gcAttackerMI);
            }
        }
    }

    setNextTime(m_Tick);


    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectProminence::unaffect()

{
    __BEGIN_TRY


    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);


    __END_CATCH
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectProminence::toString() const {
    __BEGIN_TRY

    StringStream msg;

    msg << "EffectProminence(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();

    __END_CATCH
}

void EffectProminenceLoader::load(Zone* pZone)

{
    __BEGIN_TRY

    vector<ZoneEffectRow> rows =
        defaultZoneInfoRepository().loadZoneEffectRects(pZone->getZoneID(), (int)Effect::EFFECT_CLASS_PROMINENCE_3);

    for (size_t r = 0; r < rows.size(); r++) {
        ZoneCoord_t left = rows[r].left;
        ZoneCoord_t top = rows[r].top;
        ZoneCoord_t right = rows[r].right;
        ZoneCoord_t bottom = rows[r].bottom;
        int value2 = rows[r].value2;
        int value3 = rows[r].value3;

        VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

        for (int X = left; X <= right; X++)
            for (int Y = top; Y <= bottom; Y++) {
                if (rect.ptInRect(X, Y)) {
                    Tile& tile = pZone->getTile(X, Y);
                    if (tile.canAddEffect()) {
                        EffectProminence* pEffect = new EffectProminence(pZone, X, Y);
                        pEffect->setTick(value2);
                        pEffect->setDamage(value3);
                        pEffect->setNextTime(0);
                        pEffect->setForce(true);
                        pEffect->setSendEffectClass(Effect::EFFECT_CLASS_PROMINENCE_3);

                        // Register the effect in the zone and add it to the tile.
                        pZone->registerObject(pEffect);
                        tile.addEffect(pEffect);
                    }
                }
            }
    }

    __END_CATCH
}
