//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTurretLaser.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectTurretLaser.h"

#include "Creature.h"
#include "GCAddEffectToTile.h"
#include "GCModifyInformation.h"
#include "Player.h"
#include "SkillUtil.h"
#include "Tile.h"
#include "Zone.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectTurretLaser::EffectTurretLaser(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    m_pZone = pZone;
    m_X = X;
    m_Y = Y;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTurretLaser::affect()

{
    Assert(m_pZone != NULL);

    setNextTime(5);

    affect(m_pZone, m_X, m_Y);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTurretLaser::affect(Zone* pZone, ZoneCoord_t Cx, ZoneCoord_t Cy)

{
    if (!isValidZoneCoord(pZone, Cx, Cy))
        return;
    Tile& rTile = pZone->getTile(Cx, Cy);
    forward_list<Object*>& oList = rTile.getObjectList();
    forward_list<Object*>::iterator itr = oList.begin();

    for (; itr != oList.end(); ++itr) {
        Object* pObject = *itr;
        if (pObject->getObjectClass() != Object::OBJECT_CLASS_CREATURE)
            continue;

        Creature* pCreature = dynamic_cast<Creature*>(pObject);
        if (pCreature->isMonster() || pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_REINFORCE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_SIEGE_DEFENDER))
            continue;

        GCModifyInformation gcMI;

        setDamage(pCreature, 50, NULL, SKILL_ATTACK_MELEE, &gcMI, NULL, true);
        if (pCreature->isPC())
            pCreature->getPlayer()->sendPacket(&gcMI);

        GCAddEffectToTile gcAE;
        gcAE.setObjectID(0);
        gcAE.setDuration(2);
        gcAE.setEffectID(Effect::EFFECT_CLASS_TURRET_LASER_ATTACK);

        for (int i = -1; i <= 1; ++i) {
            gcAE.setXY(Cx + i, Cy + i);
            pZone->broadcastPacket(Cx + i, Cy + i, &gcAE);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTurretLaser::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectTurretLaser::toString() const

{
    StringStream msg;
    msg << "EffectTurretLaser(" << ")";
    return msg.toString();
}
