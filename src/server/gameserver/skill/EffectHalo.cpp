//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectHalo.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectHalo.h"

#include "DB.h"
#include "GCAddEffectToTile.h"
#include "GCDeleteEffectFromTile.h"
#include "GamePlayer.h"
#include "Monster.h"
#include "Ousters.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "ZoneUtil.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectHalo::EffectHalo(Zone* pZone, ZoneCoord_t zoneX, ZoneCoord_t zoneY)

{
    m_pZone = pZone;
    m_X = zoneX;
    m_Y = zoneY;
    m_UserOID = 0;
    m_TargetOID = 0;
    m_Damage = 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHalo::affect()

{
    setNextTime(10);

    Creature* pCreature = m_pZone->getCreature(m_TargetOID);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHalo::affect(Creature* pCreature)

{
    if (pCreature == NULL)
        return;

    Zone* pZone = m_pZone;
    Assert(pZone != NULL);

    if (abs((int)pCreature->getX() - (int)m_X) > 6 || abs((int)pCreature->getY() - (int)m_Y) > 6)
        return;

    Creature* pCastCreature = pZone->getCreature(m_UserOID);

    GCModifyInformation gcMI, gcAttackerMI;

    if (canAttack(pCastCreature, pCreature) && !(pZone->getZoneLevel() & COMPLETE_SAFE_ZONE)) {
        if (pCreature->isPC()) {
            PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

            ::setDamage(pPC, getDamage(), pCastCreature, SKILL_HALO, &gcMI, &gcAttackerMI);
            pPC->getPlayer()->sendPacket(&gcMI);
        } else if (pCreature->isMonster()) {
            ::setDamage(pCreature, getDamage(), pCastCreature, SKILL_HALO, NULL, &gcAttackerMI);
        }

        if (pCastCreature != NULL && pCastCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCastCreature);

            computeAlignmentChange(pCreature, getDamage(), pOusters, &gcMI, &gcAttackerMI);
            increaseAlignment(pOusters, pCreature, gcAttackerMI);

            if (pCreature->isDead()) {
                int exp = computeCreatureExp(pCreature, 100, pOusters);
                shareOustersExp(pOusters, exp, gcAttackerMI);
            }

            pOusters->getPlayer()->sendPacket(&gcAttackerMI);
        }
    }

    GCAddEffectToTile gcAET;
    gcAET.setEffectID(getSendEffectClass());
    gcAET.setDuration(15);
    gcAET.setObjectID(pCreature->getObjectID());
    gcAET.setXY(m_X, m_Y);

    pZone->broadcastPacket(m_X, m_Y, &gcAET);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHalo::unaffect(Creature* pCreature)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectHalo::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    tile.deleteEffect(m_ObjectID);

    GCDeleteEffectFromTile gcDT;
    gcDT.setXY(m_X, m_Y);
    gcDT.setObjectID(getObjectID());
    gcDT.setEffectID(Effect::EFFECT_CLASS_HALO);
    m_pZone->broadcastPacket(m_X, m_Y, &gcDT);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectHalo::toString() const {
    StringStream msg;

    msg << "EffectHalo(" << "ObjectID:" << getObjectID() << ")";

    return msg.toString();
}
