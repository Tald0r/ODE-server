//////////////////////////////////////////////////////////////////////////////
// Filename    : TornadoSever.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "TornadoSever.h"

#include "SimpleTileMeleeSkill.h"

//////////////////////////////////////////////////////////////////////////////
// Constructor
// Initializes the mask.
//////////////////////////////////////////////////////////////////////////////
TornadoSever::TornadoSever() {
    m_pTornadoSeverMask[0].set(0, 0);
    m_pTornadoSeverMask[1].set(-1, -1);
    m_pTornadoSeverMask[2].set(0, -1);
    m_pTornadoSeverMask[3].set(1, -1);
    m_pTornadoSeverMask[4].set(-1, 0);
    m_pTornadoSeverMask[5].set(1, 0);
    m_pTornadoSeverMask[6].set(-1, 1);
    m_pTornadoSeverMask[7].set(0, 1);
    m_pTornadoSeverMask[8].set(1, 1);
}

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void TornadoSever::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Zone* pZone = pSlayer->getZone();
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        execute(pSlayer, pTargetCreature->getX(), pTargetCreature->getY(), pSkillSlot, CEffectID);
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

//////////////////////////////////////////////////////////////////////
//
// TornadoSever::execute()
//
//////////////////////////////////////////////////////////////////////
void TornadoSever::execute(Slayer* pSlayer, ZoneCoord_t X, ZoneCoord_t Y, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    SkillInput input(pSlayer, pSkillSlot);
    SkillOutput output;
    computeOutput(input, output);

    SIMPLE_SKILL_INPUT param;
    param.SkillType = getSkillType();
    param.SkillDamage = output.Damage;
    param.Delay = output.Delay;
    param.ItemClass = Item::ITEM_CLASS_BLADE;
    param.STRMultiplier = 8;
    param.DEXMultiplier = 1;
    param.INTMultiplier = 1;
    param.bMagicHitRoll = false;
    param.bMagicDamage = false;
    param.bAdd = true;

    SIMPLE_SKILL_OUTPUT result;

    for (int i = 0; i < 9; i++) {
        param.addMask(m_pTornadoSeverMask[i].x, m_pTornadoSeverMask[i].y, 100);
    }

    g_SimpleTileMeleeSkill.execute(pSlayer, X, Y, pSkillSlot, param, result);
}

TornadoSever g_TornadoSever;
