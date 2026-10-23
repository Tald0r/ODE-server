//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectTrapInstalled.cpp
// Written by  : bezz
//////////////////////////////////////////////////////////////////////////////

#include "EffectTrapInstalled.h"

#include "EffectTrapTriggered.h"
#include "GCAddEffect.h"
#include "GCDeleteEffectFromTile.h"
#include "GCModifyInformation.h"
#include "GameContext.h"
#include "Monster.h"
#include "Player.h"
#include "SkillInfo.h"
#include "SkillUtil.h"
#include "Tile.h"
#include "Zone.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectTrapInstalled::EffectTrapInstalled(Zone* pZone, ZoneCoord_t X, ZoneCoord_t Y)

{
    m_pZone = pZone;
    m_X = X;
    m_Y = Y;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTrapInstalled::affect()

{
    Assert(m_pZone != NULL);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTrapInstalled::affect(Zone* pZone, ZoneCoord_t Cx, ZoneCoord_t Cy)

{
    Assert(pZone != NULL);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTrapInstalled::affect(Creature* pCreature)

{
    if (pCreature == NULL)
        return;
    if (m_pZone == NULL)
        return;
    if (pCreature->getX() != m_X || pCreature->getY() != m_Y)
        return;

    Creature* pCaster = m_pZone->getCreature(m_UserOID);

    if (pCaster != NULL && pCaster->isSlayer()) {
        GCModifyInformation gcAttackerMI;
        Slayer* pCastSlayer = dynamic_cast<Slayer*>(pCaster);

        SkillSlot* pSkillSlot = pCastSlayer->getSkill(SKILL_INSTALL_TRAP);
        if (pSkillSlot != NULL) {
            SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_INSTALL_TRAP);
            SkillDomainType_t DomainType = pSkillInfo->getDomainType();
            SkillGrade Grade =
                de::gameContext().skillInfos().getGradeByDomainLevel(pCastSlayer->getSkillDomainLevel(DomainType));
            Exp_t ExpUp = 10 * (Grade + 1);

            // Give experience when the trap is triggered.
            shareAttrExp(pCastSlayer, ExpUp, 1, 8, 1, gcAttackerMI);
            increaseDomainExp(pCastSlayer, DomainType, pSkillInfo->getPoint(), gcAttackerMI);
            increaseSkillExp(pCastSlayer, DomainType, pSkillSlot, pSkillInfo, gcAttackerMI);

            pCastSlayer->getPlayer()->sendPacket(&gcAttackerMI);
        }
    }

    if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        if (pMonster->isMaster())
            return;
    }

    EffectTrapTriggered* pEffect = new EffectTrapTriggered(pCreature);
    pEffect->setDeadline(m_Tick);

    pCreature->setFlag(Effect::EFFECT_CLASS_TRAPPED);
    pCreature->addEffect(pEffect);

    GCAddEffect gcAddEffect;
    gcAddEffect.setObjectID(pCreature->getObjectID());
    gcAddEffect.setEffectID(Effect::EFFECT_CLASS_TRAP_TRIGGERED);
    gcAddEffect.setDuration(m_Tick);

    m_pZone->broadcastPacket(m_X, m_Y, &gcAddEffect);
    setDeadline(0);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectTrapInstalled::unaffect()

{
    Tile& tile = m_pZone->getTile(m_X, m_Y);
    if (!tile.hasEffect())
        return;

    tile.deleteEffect(m_ObjectID);

    GCDeleteEffectFromTile gcDeleteEffect;
    gcDeleteEffect.setXY(m_X, m_Y);
    gcDeleteEffect.setObjectID(getObjectID());
    gcDeleteEffect.setEffectID(getSendEffectClass());

    m_pZone->broadcastPacket(m_X, m_Y, &gcDeleteEffect);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectTrapInstalled::toString() const {
    StringStream msg;
    msg << "EffectTrapInstalled(" << ")";
    return msg.toString();
}
