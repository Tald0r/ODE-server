//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHeavenGround.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectHeavenGround.h"

#include "DB.h"
#include "EffectTrapTriggered.h"
#include "GCAddEffect.h"
#include "GCDeleteEffectFromTile.h"
#include "GCModifyInformation.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Tile.h"
#include "Vampire.h"
#include "Zone.h"
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHeavenGround::EffectHeavenGround(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    m_pZone = pZone;
    m_X = X;
    m_Y = Y;
    m_UserObjectID = 0;
    m_bSlayer = false;
    m_Damage = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHeavenGround::affect()

{
    Assert(m_pZone != NULL);
    //  Get the creature that cast the skill.
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);

    if (pCastCreature == NULL) {
        setNextTime(m_Tick);

        return;
    }

    // Get the tile.
    Tile& tile = m_pZone->getTile(m_X, m_Y);

    // Get the tile's object list.
    const forward_list<Object*>& oList = tile.getObjectList();
    forward_list<Object*>::const_iterator itr = oList.begin();
    for (; itr != oList.end(); itr++) {
        Assert(*itr != NULL);

        Object* pObject = *itr;
        Assert(pObject != NULL);

        if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
            Creature* pCreature = dynamic_cast<Creature*>(pObject);
            Assert(pCreature != NULL);

            // Check whether the current target can be attacked.
            if (!canAttack(pCastCreature, pCreature) || pCreature->isFlag(Effect::EFFECT_CLASS_IMMUNE_TO_ACID) ||
                pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                continue;
            }

            // Check the zone level.
            if (!checkZoneLevelToHitTarget(pCreature))
                continue;
            // Compute the damage.
            int AcidDamage = computeMagicDamage(pCreature, m_Damage, SKILL_HEAVEN_GROUND, m_bSlayer, pCastCreature);
            // Flying targets are not attacked.
            if (pCreature->getMoveMode() != Creature::MOVE_MODE_FLYING) {
                GCModifyInformation gcAttackerMI;
                GCModifyInformation gcDefenderMI;
                // A human target takes no damage.
                if (pCreature->isSlayer()) {
                    continue;
                } else if (pCreature->isVampire()) {
                    Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                    ::setDamage(pVampire, AcidDamage, pCastCreature, SKILL_HEAVEN_GROUND, &gcDefenderMI, &gcAttackerMI,
                                false);

                    Player* pPlayer = pVampire->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isOusters()) {
                    Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                    ::setDamage(pOusters, AcidDamage, pCastCreature, SKILL_HEAVEN_GROUND, &gcDefenderMI, &gcAttackerMI,
                                false);

                    Player* pPlayer = pOusters->getPlayer();
                    Assert(pPlayer != NULL);
                    pPlayer->sendPacket(&gcDefenderMI);
                } else if (pCreature->isMonster()) {
                    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                    ::setDamage(pMonster, AcidDamage, pCastCreature, SKILL_HEAVEN_GROUND, NULL, &gcAttackerMI, false);
                }

                bool modifiedAttacker = false;

                // Grants experience when the target dies.
                // 						//computeAlignmentChange(pCreature, AcidDamage, pSlayer, &gcDefenderMI,
                // 						//decreaseDurability(pSlayer, pCreature, NULL, &gcAttackerMI, &gcDefenderMI);
                //
                // 						// Experience is granted only when the target is not a Slayer.
                // // 							//shareAttrExp(pSlayer, AcidDamage , 1, 1, 8, _GCSkillToObjectOK1);
                // // 							increaseDomainExp(pSlayer, SKILL_HEAVEN_GROUND, pSkillInfo->getPoint(),
                // // 							increaseSkillExp(pSlayer, SKILL_HEAVEN_GROUND,  pSkillSlot, pSkillInfo,
                // // 							increaseAlignment(pSlayer, pCreature, gcAttackerMI);

                // Compute the alignment change.
                if (pCastCreature != NULL && pCastCreature->isPC() && pCreature->isPC()) {
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
void EffectHeavenGround::affect(Zone* pZone, ZoneCoord_t Cx, ZoneCoord_t Cy)

{
    Assert(pZone != NULL);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHeavenGround::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);

    GCDeleteEffectFromTile gcDeleteEffect;
    gcDeleteEffect.setXY(m_X, m_Y);
    gcDeleteEffect.setObjectID(getObjectID());
    gcDeleteEffect.setEffectID(getSendEffectClass());

    m_pZone->broadcastPacket(m_X, m_Y, &gcDeleteEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHeavenGround::toString() const {
    StringStream msg;
    msg << "EffectHeavenGround(" << ")";
    return msg.toString();
}
