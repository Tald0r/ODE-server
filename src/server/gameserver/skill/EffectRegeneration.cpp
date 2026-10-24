//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectRegeneration.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectRegeneration.h"

#include "GCAddEffectToTile.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "PCFinder.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneInfoManager.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectRegeneration::EffectRegeneration(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    m_pZone = pZone;
    m_X = X;
    m_Y = Y;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRegeneration::affect()

{
    Assert(m_pZone != NULL);

    affect(m_pZone, m_X, m_Y);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRegeneration::affect(Zone* pZone, ZoneCoord_t Cx, ZoneCoord_t Cy)

{
    Assert(pZone != NULL);

    VSRect rect(0, 0, pZone->getWidth() - 1, pZone->getHeight() - 1);

    GCSkillToSelfOK1 _GCSkillToSelfOK1;
    GCSkillToSelfOK2 _GCSkillToSelfOK2;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            int X = Cx + x;
            int Y = Cy + y;

            if (!rect.ptInRect(X, Y))
                continue;

            // Fetches the objects on the tile.
            Tile& tile = pZone->getTile(X, Y);

            const forward_list<Object*>& oList = tile.getObjectList();
            for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                Object* pTargetObject = (*itr);
                if (pTargetObject != NULL && pTargetObject->getObjectClass() == Object::OBJECT_CLASS_CREATURE) {
                    Creature* pCreature = dynamic_cast<Creature*>(pTargetObject);
                    Assert(pCreature != NULL);

                    // A creature in a coma or already dead is not healed.
                    // 2003. 3. 11. Sequoia
                    if (pCreature->isSlayer() && !pCreature->isFlag(Effect::EFFECT_CLASS_COMA) &&
                        !pCreature->isDead()) {
                        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

                        HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
                        HP_t MaxHP = pSlayer->getHP(ATTR_MAX);

                        if (CurrentHP < MaxHP) {
                            HP_t RemainHP = min((int)MaxHP, (int)CurrentHP + m_Damage);
                            pSlayer->setHP(RemainHP, ATTR_CURRENT);

                            GCModifyInformation gcMI;
                            gcMI.addShortData(MODIFY_CURRENT_HP, RemainHP);
                            pSlayer->getPlayer()->sendPacket(&gcMI);

                            _GCSkillToSelfOK1.setSkillType(SKILL_CURE_EFFECT);
                            _GCSkillToSelfOK1.setDuration(0);
                            pSlayer->getPlayer()->sendPacket(&_GCSkillToSelfOK1);

                            _GCSkillToSelfOK2.setObjectID(pSlayer->getObjectID());
                            _GCSkillToSelfOK2.setSkillType(SKILL_CURE_EFFECT);
                            _GCSkillToSelfOK2.setDuration(0);
                            pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &_GCSkillToSelfOK2, pCreature);

                            GCStatusCurrentHP gcStatusCurrentHP;
                            gcStatusCurrentHP.setObjectID(pSlayer->getObjectID());
                            gcStatusCurrentHP.setCurrentHP(RemainHP);
                            pZone->broadcastPacket(X, Y, &gcStatusCurrentHP);
                        }
                    }
                }
            }
        }
    }

    setNextTime(m_Delay);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectRegeneration::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectRegeneration::toString() const {
    StringStream msg;
    msg << "EffectRegeneration(" << "Damage:" << (int)m_Damage << ")";
    return msg.toString();
}
