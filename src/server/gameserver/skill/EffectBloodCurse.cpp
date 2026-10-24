//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectBloodCurse.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectBloodCurse.h"

#include "GCModifyInformation.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectBloodCurse::EffectBloodCurse(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY, bool bPlayer)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_Damage = 0;
    m_UserObjectID = 0;
    m_bPlayer = bPlayer;

    m_SplashRatio[0] = 100;
    m_SplashRatio[1] = 85;
    m_SplashRatio[2] = 75;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodCurse::affect()

{
    Assert(m_pZone != NULL);

    // Gets the creature that cast the effect.
    // It may have left the zone, so this can be NULL.
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);
    if (m_bPlayer) {
        if (pCastCreature == NULL) {
            setDeadline(0);
            return;
        }
    }

    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);

    // Get the tile this effect is attached to.
    // Center tile plus the splash tiles.
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            int X = m_X + x;
            int Y = m_Y + y;

            if (!rect.ptInRect(X, Y))
                continue;
            Tile& tile = m_pZone->getTile(X, Y);

            int Damage = 0;
            int splash = max(abs(x), abs(y));

            // The center takes 100%.

            if (splash >= 3)
                splash = 2;
            Damage = getPercentValue(m_Damage, m_SplashRatio[splash]);

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

                    // The caster itself is not hit.
                    // Check for invulnerability.
                    if (pCreature->getObjectID() == m_UserObjectID || !canAttack(pCastCreature, pCreature) ||
                        pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || !checkZoneLevelToHitTarget(pCreature)) {
                        continue;
                    }

                    if (pCastCreature != NULL && pCastCreature->isMonster()) {
                        Monster* pMonster = dynamic_cast<Monster*>(pCastCreature);
                        if (pMonster != NULL && !pMonster->isEnemyToAttack(pCreature))
                            continue;
                    }

                    GCModifyInformation gcAttackerMI;
                    GCSkillToObjectOK2 gcSkillToObjectOK2;

                    if (pCreature->isSlayer()) {
                        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                        ::setDamage(pSlayer, Damage, pCastCreature, SKILL_BLOOD_CURSE, &gcSkillToObjectOK2,
                                    &gcAttackerMI);


                    } else if (pCreature->isVampire()) {
                        // When a player cast it, Vampires are not hit at all.
                        if (m_bPlayer) // && splash != 0 )
                            continue;

                        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                        ::setDamage(pVampire, Damage, pCastCreature, SKILL_BLOOD_CURSE, &gcSkillToObjectOK2,
                                    &gcAttackerMI);

                    } else if (pCreature->isOusters()) {
                        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                        ::setDamage(pOusters, Damage, pCastCreature, SKILL_BLOOD_CURSE, &gcSkillToObjectOK2,
                                    &gcAttackerMI);

                    } else if (pCreature->isMonster()) {
                        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                        ::setDamage(pMonster, Damage, pCastCreature, SKILL_BLOOD_CURSE, NULL, &gcAttackerMI);

                        if (pCastCreature != NULL)
                            pMonster->addEnemy(pCastCreature);
                    }

                    // Grant experience if the target died.
                    if (pCreature->isDead()) {
                        if (pCastCreature != NULL && pCastCreature->isVampire()) {
                            Vampire* pVampire = dynamic_cast<Vampire*>(pCastCreature);
                            Assert(pVampire != NULL);

                            int exp = computeCreatureExp(pCreature, KILL_EXP);
                            shareVampExp(pVampire, exp, gcAttackerMI);
                            computeAlignmentChange(pCreature, Damage, pCastCreature, &gcSkillToObjectOK2,
                                                   &gcAttackerMI);

                            pVampire->getPlayer()->sendPacket(&gcAttackerMI);
                        }
                    }

                    // Show the target player the hit animation.
                    if (pCreature->isPC()) {
                        gcSkillToObjectOK2.setObjectID(1); // Has no meaning.
                        gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                        gcSkillToObjectOK2.setDuration(0);
                        pCreature->getPlayer()->sendPacket(&gcSkillToObjectOK2);
                    }

                    GCSkillToObjectOK4 gcSkillToObjectOK4;
                    gcSkillToObjectOK4.setTargetObjectID(pCreature->getObjectID());
                    gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                    gcSkillToObjectOK4.setDuration(0);

                    m_pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcSkillToObjectOK4, pCreature);
                }
            }
        }
    }

    setDeadline(0);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodCurse::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodCurse::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectBloodCurse::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectBloodCurse::toString() const {
    StringStream msg;

    msg << "EffectBloodCurse(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
