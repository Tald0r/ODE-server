//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDecreaseHP.cpp
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDecreaseHP.h"

#include "DB.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "Monster.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDecreaseHP::EffectDecreaseHP(Creature* pCreature)

{
    m_UserObjectID = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::affect(Creature* pCreature)

{}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG

    Assert(pCreature != NULL);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    pCreature->removeFlag(Effect::EFFECT_CLASS_DECREASE_HP);

    Damage_t decreaseHP = m_Point;

    if (!(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE) && !pCreature->isDead() &&
        !pCreature->isFlag(Effect::EFFECT_CLASS_COMA)
        // Invincibility check.
        && canAttack(NULL, pCreature)) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

            HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);

            if (CurrentHP > 0) {
                HP_t RemainHP = max(0, CurrentHP - (int)decreaseHP);

                pSlayer->setHP(RemainHP, ATTR_CURRENT);

                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_CURRENT_HP, RemainHP);
                pSlayer->getPlayer()->sendPacket(&gcMI);

                // Broadcasts the changed HP.
                GCStatusCurrentHP pkt;
                pkt.setObjectID(pSlayer->getObjectID());
                pkt.setCurrentHP(RemainHP);
                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &pkt);
            }
        } else if (pCreature->isVampire()) {
            Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

            HP_t CurrentHP = pVampire->getHP(ATTR_CURRENT);

            if (CurrentHP > 0) {
                HP_t RemainHP = max(0, CurrentHP - (int)decreaseHP);

                pVampire->setHP(RemainHP, ATTR_CURRENT);

                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_CURRENT_HP, RemainHP);
                pVampire->getPlayer()->sendPacket(&gcMI);

                // When attacked (blood drain), the attacker alignment changes.
                Creature* pAttacker = pZone->getCreature(m_UserObjectID);
                if (pAttacker != NULL && pAttacker->isVampire()) {
                    Vampire* pAttackVampire = dynamic_cast<Vampire*>(pAttacker);

                    GCModifyInformation gcAttackerMI;
                    computeAlignmentChange(pVampire, decreaseHP, pAttackVampire, NULL, &gcAttackerMI);

                    // Sends the information if anything changed.
                    if (gcAttackerMI.getShortCount() + gcAttackerMI.getLongCount() > 0) {
                        pAttackVampire->getPlayer()->sendPacket(&gcAttackerMI);
                    }
                }

                // Broadcasts the changed HP.
                GCStatusCurrentHP pkt;
                pkt.setObjectID(pVampire->getObjectID());
                pkt.setCurrentHP(RemainHP);
                pZone->broadcastPacket(pVampire->getX(), pVampire->getY(), &pkt);
            }
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

            HP_t CurrentHP = pOusters->getHP(ATTR_CURRENT);

            if (CurrentHP > 0) {
                HP_t RemainHP = max(0, CurrentHP - (int)decreaseHP);

                pOusters->setHP(RemainHP, ATTR_CURRENT);

                GCModifyInformation gcMI;
                gcMI.addShortData(MODIFY_CURRENT_HP, RemainHP);
                pOusters->getPlayer()->sendPacket(&gcMI);

                // When attacked (blood drain), the attacker alignment changes.
                Creature* pAttacker = pZone->getCreature(m_UserObjectID);
                if (pAttacker != NULL && pAttacker->isOusters()) {
                    Ousters* pAttackOusters = dynamic_cast<Ousters*>(pAttacker);

                    GCModifyInformation gcAttackerMI;
                    computeAlignmentChange(pOusters, decreaseHP, pAttackOusters, NULL, &gcAttackerMI);

                    // Sends the information if anything changed.
                    if (gcAttackerMI.getShortCount() + gcAttackerMI.getLongCount() > 0) {
                        pAttackOusters->getPlayer()->sendPacket(&gcAttackerMI);
                    }
                }

                // Broadcasts the changed HP.
                GCStatusCurrentHP pkt;
                pkt.setObjectID(pOusters->getObjectID());
                pkt.setCurrentHP(RemainHP);
                pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &pkt);
            }
        } else if (pCreature->isMonster()) {
            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

            HP_t CurrentHP = pMonster->getHP(ATTR_CURRENT);

            if (CurrentHP > 0) {
                HP_t RemainHP = max(0, CurrentHP - (int)decreaseHP);

                pMonster->setHP(RemainHP, ATTR_CURRENT);

                // Broadcasts the changed HP.
                GCStatusCurrentHP pkt;
                pkt.setObjectID(pMonster->getObjectID());
                pkt.setCurrentHP(RemainHP);
                pZone->broadcastPacket(pMonster->getX(), pMonster->getY(), &pkt);

                if (RemainHP == 0) {
                    Creature* pAttacker = pZone->getCreature(m_UserObjectID);
                    if (pAttacker != NULL && pAttacker->isVampire()) {
                        Vampire* pAttackVampire = dynamic_cast<Vampire*>(pAttacker);

                        GCModifyInformation gcMI;
                        increaseAlignment(pAttackVampire, pCreature, gcMI);

                        if (gcMI.getShortCount() > 0 || gcMI.getLongCount() > 0)
                            pAttackVampire->getPlayer()->sendPacket(&gcMI);
                    }
                }
            }
        }


        // Handles the kill count when the caster kills pCreature.
        // by sigi. 2002.9.9
        if (pCreature->isDead()) {
            Creature* pAttacker = pZone->getCreature(m_UserObjectID);

            if (pAttacker != NULL) {
                if (pAttacker->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pAttacker);
                    // Grants experience for the kill.
                    GCModifyInformation mi;

                    int exp = computeCreatureExp(pCreature, KILL_EXP);
                    shareVampExp(pVampire, exp, mi);

                    if (pCreature->isMonster()) {
                        increaseFame(pVampire, decreaseHP);
                        mi.addLongData(MODIFY_FAME, pVampire->getFame());
                    }

                    pAttacker->getPlayer()->sendPacket(&mi);
                } else if (pAttacker->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pAttacker);
                    GCModifyInformation mi;
                    int exp = computeCreatureExp(pCreature, 100);
                    shareOustersExp(pOusters, exp, mi);

                    if (pCreature->isMonster()) {
                        increaseFame(pOusters, decreaseHP);
                        mi.addLongData(MODIFY_FAME, pOusters->getFame());
                    }
                }

                affectKillCount(pAttacker, pCreature);
            }
        }
    }

    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDecreaseHP::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectDecreaseHP::toString() const {
    StringStream msg;
    msg << "EffectDecreaseHP(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
