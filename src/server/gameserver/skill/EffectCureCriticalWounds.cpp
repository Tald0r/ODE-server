//////////////////////////////////////////////////////////////////////////////
// Filename    : EffectCureCriticalWounds.cpp
// Written by  : elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "EffectCureCriticalWounds.h"

#include "EffectAftermath.h"
#include "EffectBloodDrain.h"
#include "GCModifyInformation.h"
#include "GCRemoveEffect.h"
#include "GCSkillToSelfOK1.h"
#include "GCSkillToSelfOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"
#include "HitRoll.h"
#include "Monster.h"
#include "Player.h"
#include "SkillInfo.h"
#include "SkillUtil.h"
#include "Slayer.h"
#include "Vampire.h"
#include "ZoneUtil.h"

const uint CriticalBloodDrainLevel = 74;

EffectCureCriticalWounds::EffectCureCriticalWounds(Creature* pCreature)

{
    setTarget(pCreature);
    m_Range = 0;
}

void EffectCureCriticalWounds::affect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    affect(pCreature);
}

void EffectCureCriticalWounds::affect(Creature* pCreature)

{
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCSkillToSelfOK1 _GCSkillToSelfOK1;
    GCSkillToSelfOK2 _GCSkillToSelfOK2;

    int X = pCreature->getX();
    int Y = pCreature->getY();
    Exp_t ExpUp = 0;
    bool bCured = false;

    for (int oY = -2; oY <= 2; oY++)
        for (int oX = -2; oX <= 2; oX++) {
            int tileX = X + oX;
            int tileY = Y + oY;

            if (isValidZoneCoord(pZone, tileX, tileY)) {
                Tile& tile = pZone->getTile(tileX, tileY);

                if (tile.hasCreature(Creature::MOVE_MODE_WALKING)) {
                    const forward_list<Object*>& oList = tile.getObjectList();
                    for (forward_list<Object*>::const_iterator itr = oList.begin(); itr != oList.end(); itr++) {
                        Object* pTarget = *itr;
                        Creature* pTargetCreature = NULL;
                        if (pTarget->getObjectClass() == Object::OBJECT_CLASS_CREATURE &&
                            (pTargetCreature = dynamic_cast<Creature*>(pTarget))->isSlayer()) {
                            Assert(pTargetCreature != NULL);
                            HP_t RemainHP = 0;

                            if (pTargetCreature->isSlayer() && !pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                                Slayer* pSlayer = dynamic_cast<Slayer*>(pTargetCreature);
                                HP_t CurrentHP = pSlayer->getHP(ATTR_CURRENT);
                                HP_t MaxHP = pSlayer->getHP(ATTR_MAX);

                                if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_BLOOD_DRAIN)) {
                                    ExpUp++;
                                    Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);
                                    EffectBloodDrain* pEffectBloodDrain = dynamic_cast<EffectBloodDrain*>(pEffect);

                                    Assert(pEffectBloodDrain != NULL);

                                    if (pEffectBloodDrain->getLevel() < CriticalBloodDrainLevel) {
                                        // Attaches an aftermath effect that prevents blood drain farming.
                                        if (pSlayer->isFlag(Effect::EFFECT_CLASS_AFTERMATH)) {
                                            Effect* pEffect = pSlayer->findEffect(Effect::EFFECT_CLASS_AFTERMATH);
                                            EffectAftermath* pEffectAftermath = dynamic_cast<EffectAftermath*>(pEffect);
                                            pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
                                        } else {
                                            EffectAftermath* pEffectAftermath = new EffectAftermath(pSlayer);
                                            pEffectAftermath->setDeadline(5 * 600); // Lasts for 5 minutes.
                                            pSlayer->addEffect(pEffectAftermath);
                                            pSlayer->setFlag(Effect::EFFECT_CLASS_AFTERMATH);
                                            pEffectAftermath->create(pSlayer->getName());
                                        }

                                        pEffectBloodDrain->destroy(pSlayer->getName());
                                        pSlayer->deleteEffect(Effect::EFFECT_CLASS_BLOOD_DRAIN);

                                        bCured = true;

                                        SLAYER_RECORD prev;
                                        pSlayer->getSlayerRecord(prev);
                                        pSlayer->initAllStat();
                                        pSlayer->sendRealWearingInfo();
                                        pSlayer->sendModifyInfo(prev);

                                        GCRemoveEffect gcRemoveEffect;
                                        gcRemoveEffect.setObjectID(pSlayer->getObjectID());
                                        gcRemoveEffect.addEffectList((EffectID_t)Effect::EFFECT_CLASS_BLOOD_DRAIN);
                                        pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &gcRemoveEffect);
                                    }
                                }

                                if (CurrentHP < MaxHP) {
                                    ExpUp++;
                                    bCured = true;

                                    RemainHP = min(CurrentHP + m_Point, (int)MaxHP);
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
                                    pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(),
                                                           &_GCSkillToSelfOK2, pTargetCreature);

                                    Zone* pZone = pTargetCreature->getZone();
                                    GCStatusCurrentHP gcStatusCurrentHP;
                                    gcStatusCurrentHP.setObjectID(pTargetCreature->getObjectID());
                                    gcStatusCurrentHP.setCurrentHP(RemainHP);
                                    pZone->broadcastPacket(pTargetCreature->getX(), pTargetCreature->getY(),
                                                           &gcStatusCurrentHP);
                                }
                            }
                        }
                    }
                }
            }
        }

    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_CURE_CRITICAL_WOUNDS);

    if (pSkillInfo != NULL && bCured) {
        SkillSlot* pSkillSlot = ((Slayer*)pCreature)->hasSkill(SKILL_CURE_CRITICAL_WOUNDS);
        if (pSkillSlot != NULL) {
            Slayer* pCastSlayer = dynamic_cast<Slayer*>(pCreature);
            GCModifyInformation gcMI;
            SkillDomainType_t DomainType = pSkillInfo->getDomainType();
            // Raises experience.
            shareAttrExp(pCastSlayer, ExpUp, 1, 1, 8, gcMI);
            increaseDomainExp(pCastSlayer, DomainType, ExpUp, gcMI);
            increaseSkillExp(pCastSlayer, DomainType, pSkillSlot, pSkillInfo, gcMI);
            pCastSlayer->getPlayer()->sendPacket(&gcMI);
        }
    }

    setNextTime(m_Delay);
}

void EffectCureCriticalWounds::affect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

void EffectCureCriticalWounds::unaffect(Creature* pCreature)

{
    Assert(pCreature != NULL);
    pCreature->removeFlag(Effect::EFFECT_CLASS_CURE_CRITICAL_WOUNDS);
    Zone* pZone = pCreature->getZone();
    Assert(pZone != NULL);

    GCRemoveEffect gcRemoveEffect;
    gcRemoveEffect.setObjectID(pCreature->getObjectID());
    gcRemoveEffect.addEffectList(Effect::EFFECT_CLASS_CURE_CRITICAL_WOUNDS);
    pZone->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcRemoveEffect);
}

void EffectCureCriticalWounds::unaffect()

{
    Creature* pCreature = dynamic_cast<Creature*>(m_pTarget);
    unaffect(pCreature);
}

void EffectCureCriticalWounds::unaffect(Zone* pZone, ZoneCoord_t x, ZoneCoord_t y, Object* pObject)

{}

string EffectCureCriticalWounds::toString() const {
    StringStream msg;
    msg << "EffectCureCriticalWounds(" << "ObjectID:" << getObjectID() << ")";
    return msg.toString();
}
