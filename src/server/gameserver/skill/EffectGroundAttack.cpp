//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectGroundAttack.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectGroundAttack.h"

#include "GCModifyInformation.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectGroundAttack::EffectGroundAttack(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_DamagePercent = 0;
    m_UserObjectID = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGroundAttack::affect()

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGroundAttack::affect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGroundAttack::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectGroundAttack::unaffect()

{
    Assert(m_pZone != NULL);

    // Gets the caster.
    // It can be NULL, since the caster may already have left the zone.
    // by bezz. 2003.1.4
    Creature* pCastCreature = m_pZone->getCreature(m_UserObjectID);

    int Damage;
    int DamageLimit = 0xFFFF; // 500;

    HP_t MaxHP = 0;

    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1);

    // Get the tile this effect is attached to.
    // Center tile plus the splash tiles.
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            int X = m_X + x;
            int Y = m_Y + y;

            if (!rect.ptInRect(X, Y))
                continue;
            Tile& tile = m_pZone->getTile(X, Y);

            // The center takes 100%.
            // 50% damage around it
            bool bCenterEffect = (x == 0 && y == 0);
            int DamagePercent = (bCenterEffect ? m_DamagePercent : (m_DamagePercent >> 1));

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
                    if (!canAttack(pCastCreature, pCreature)) {
                        continue;
                    }

                    if (pCreature->isSlayer()) {
                        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                        MaxHP = pSlayer->getHP(ATTR_MAX);
                        Damage = min(DamageLimit, MaxHP * DamagePercent / 100);

                        GCModifyInformation gcMI;
                        ::setDamage(pSlayer, Damage, pCastCreature, SKILL_GROUND_ATTACK, &gcMI);

                        Player* pPlayer = pSlayer->getPlayer();
                        Assert(pPlayer != NULL);
                        pPlayer->sendPacket(&gcMI);
                    } else if (pCreature->isVampire()) {
                        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

                        MaxHP = pVampire->getHP(ATTR_MAX);
                        Damage = min(DamageLimit, MaxHP * DamagePercent / 100);

                        GCModifyInformation gcMI;
                        ::setDamage(pVampire, Damage, pCastCreature, SKILL_GROUND_ATTACK, &gcMI);

                        Player* pPlayer = pVampire->getPlayer();
                        Assert(pPlayer != NULL);
                        pPlayer->sendPacket(&gcMI);
                    } else if (pCreature->isOusters()) {
                        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

                        MaxHP = pOusters->getHP(ATTR_MAX);
                        Damage = min(DamageLimit, MaxHP * DamagePercent / 100);

                        GCModifyInformation gcMI;
                        ::setDamage(pOusters, Damage, pCastCreature, SKILL_GROUND_ATTACK, &gcMI);

                        Player* pPlayer = pOusters->getPlayer();
                        Assert(pPlayer != NULL);
                        pPlayer->sendPacket(&gcMI);
                    } else if (pCreature->isMonster()) {
                        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                        MaxHP = pMonster->getHP(ATTR_MAX);
                        Damage = min(DamageLimit, MaxHP * DamagePercent / 100);

                        ::setDamage(pMonster, Damage, pCastCreature, SKILL_GROUND_ATTACK);
                    }

                    // Show the target player the hit animation.
                    if (pCreature->isPC()) {
                        GCSkillToObjectOK2 gcSkillToObjectOK2;
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

    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectGroundAttack::toString() const {
    StringStream msg;

    msg << "EffectGroundAttack(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
