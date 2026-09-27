//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectCannonade.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectCannonade.h"

#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK4.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "HitRoll.h"
#include "Monster.h"
#include "PCFinder.h"
#include "SkillInfo.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectCannonade::EffectCannonade(Creature* pCreature, Zone* pZone, ZoneCoord_t x, ZoneCoord_t y)

{
    __BEGIN_TRY

    setTarget(pCreature);
    m_pZone = pZone;
    m_X = x;
    m_Y = y;
    setBroadcastingEffect(false);

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectCannonade::affect(Creature* pCreature)

{
    __BEGIN_TRY
    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectCannonade::unaffect(Creature* pCastCreature)

{
    __BEGIN_TRY

    Assert(pCastCreature != NULL);

    VSRect rect(0, 0, m_pZone->getWidth() - 1, m_pZone->getHeight() - 1); // Last valid tile coordinates

    GCSkillToObjectOK2 gcSkillToObjectOK2;
    GCSkillToObjectOK4 gcSkillToObjectOK4;

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            int X = m_X + x;
            int Y = m_Y + y;

            if (!rect.ptInRect(X, Y))
                continue;

            // Fetches the objects on the tile.
            Tile& tile = m_pZone->getTile(X, Y);
            const forward_list<Object*>& oList = tile.getObjectList();
            forward_list<Object*>::const_iterator itr = oList.begin();

            Damage_t Damage = m_Damage;

            for (; itr != oList.end(); itr++) {
                Object* pObject = *itr;
                Assert(pObject != NULL);

                if (pObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    Creature* pCreature = dynamic_cast<Creature*>(pObject);
                    Assert(pCreature != NULL);

                    // Skips the target itself, anything that cannot be attacked, and creatures in a coma.
                    if (pCreature == m_pTarget || !canAttack(pCastCreature, pCreature) ||
                        pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                        continue;
                    }

                    bool bPK = verifyPK(pCastCreature, pCreature);
                    bool bZoneLevelCheck = checkZoneLevelToHitTarget(pCreature);

                    if (pCastCreature->isMonster()) {
                        Monster* pMonster = dynamic_cast<Monster*>(pCastCreature);
                        Assert(pMonster != NULL);
                        bPK = pMonster->isEnemyToAttack(pCreature);
                    }

                    bool bHitRoll = HitRoll::isSuccess(pCastCreature, pCreature);

                    if (bPK && bZoneLevelCheck && bHitRoll) {
                        // Computes the final damage: the base damage plus the skill damage bonus.
                        Damage_t FinalDamage = 0;
                        FinalDamage = computeDamage(pCastCreature, pCreature);
                        FinalDamage += Damage;

                        if (pCreature->isPC() && pCreature->getCreatureClass() != pCastCreature->getCreatureClass()) {
                            GCModifyInformation gcMI;
                            ::setDamage(pCreature, FinalDamage, pCastCreature, SKILL_CANNONADE,
                                        &gcMI); // Global setDamage

                            pCreature->getPlayer()->sendPacket(&gcMI);

                            // Shows the hit animation.
                            gcSkillToObjectOK2.setObjectID(1); // Unused
                            gcSkillToObjectOK2.setSkillType(SKILL_ATTACK_MELEE);
                            gcSkillToObjectOK2.setDuration(0);
                            pCreature->getPlayer()->sendPacket(&gcSkillToObjectOK2);
                        } else if (pCreature->isMonster()) {
                            Monster* pMonster = dynamic_cast<Monster*>(pCreature);

                            ::setDamage(pMonster, FinalDamage, pCastCreature, SKILL_CANNONADE); // Global setDamage

                            pMonster->addEnemy(pCastCreature);
                        } else
                            continue;

                        gcSkillToObjectOK4.setTargetObjectID(pCreature->getObjectID());
                        gcSkillToObjectOK4.setSkillType(SKILL_ATTACK_MELEE);
                        gcSkillToObjectOK4.setDuration(0);
                        m_pZone->broadcastPacket(X, Y, &gcSkillToObjectOK4, pCreature);
                    }
                }
            }
        }
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectCannonade::unaffect()

{
    __BEGIN_TRY

    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget); // by Sequoia

    Assert(pCreature != NULL);

    if (m_pZone != NULL && m_pZone == pCreature->getZone()) {
        unaffect(pCreature);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectCannonade::toString() const {
    __BEGIN_TRY

    StringStream msg;
    msg << "EffectCannonade("
        << "Zone:" << de::gameContext().zoneInfos().getZoneInfo(m_pZone->getZoneID())->getFullName()
        << ",X:" << (int)m_X << ",Y:" << (int)m_Y << ",Damage:" << (int)m_Damage << ")";
    return msg.toString();

    __END_CATCH
}
