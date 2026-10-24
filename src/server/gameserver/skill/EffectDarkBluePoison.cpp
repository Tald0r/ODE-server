//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectDarkBluePoison.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectDarkBluePoison.h"

#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "Monster.h"
#include "Player.h"
#include "SkillHandler.h"
#include "Slayer.h"
#include "Vampire.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
EffectDarkBluePoison::EffectDarkBluePoison(Creature* pCreature)

{
    m_Level = 0;
    setTarget(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::affect(Creature* pCreature)

{
    Assert(pCreature != NULL);

    if (canAttack(NULL, pCreature)) {
        // Only a Slayer takes poison damage.
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Assert(pSlayer != NULL);


            MP_t CurrentMP = pSlayer->getMP(ATTR_CURRENT);
            MP_t RemainMP = max(0, (int)CurrentMP - m_Damage);

            pSlayer->setMP(RemainMP, ATTR_CURRENT);

            GCModifyInformation gcMI;
            gcMI.addShortData(MODIFY_CURRENT_MP, RemainMP);

            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);
            pPlayer->sendPacket(&gcMI);
        } else if (pCreature->isOusters()) {
            Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
            Assert(pOusters != NULL);


            // Ousters lose only half as much.
            MP_t CurrentMP = pOusters->getMP(ATTR_CURRENT);
            MP_t RemainMP = max(0, (int)CurrentMP - (m_Damage / 2));

            pOusters->setMP(RemainMP, ATTR_CURRENT);

            GCModifyInformation gcMI;
            gcMI.addShortData(MODIFY_CURRENT_MP, RemainMP);

            Player* pPlayer = pCreature->getPlayer();
            Assert(pPlayer != NULL);
            pPlayer->sendPacket(&gcMI);
        }
    }

    setNextTime(m_Tick);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::unaffect(Creature* pCreature)

{
    __BEGIN_DEBUG


    Assert(pCreature != NULL);

    // Remove the flag from the creature.
    pCreature->removeFlag(Effect::EFFECT_CLASS_DARKBLUE_POISON);

    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    if (pCreature->isPC()) {
        GCRemoveEffect gcRemoveEffect;
        gcRemoveEffect.setObjectID(pCreature->getObjectID());
        gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_DARKBLUE_POISON);

        Player* pPlayer = pCreature->getPlayer();
        Assert(pPlayer != NULL);
        pPlayer->sendPacket(&gcRemoveEffect);
    }


    __END_DEBUG
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void EffectDarkBluePoison::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
string EffectDarkBluePoison::toString() const {
    StringStream msg;
    msg << "EffectDarkBluePoison(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
