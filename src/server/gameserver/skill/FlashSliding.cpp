//////////////////////////////////////////////////////////////////////////////
// Filename    : FlashSliding.cpp
// Written by  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "FlashSliding.h"

#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCStatusCurrentHP.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void FlashSliding::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        // An NPC cannot be attacked.
        if (pTargetCreature == NULL || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        bool bIncreaseDomainExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        // The skill cannot be used when the equipped weapon is null or not a sword.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, computeSkillRange(pSkillSlot, pSkillInfo));
        bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, SkillLevel / 2);
        bool bCanHit = canHit(pSlayer, pTargetCreature, SkillType) && canAttack(pSlayer, pTargetCreature);
        bool bPK = verifyPK(pSlayer, pTargetCreature);
        bool bEffected = pSlayer->hasRelicItem() || pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_FLAG) ||
                         pSlayer->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER);

        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK && !bEffected) {
            // Moves the PC quickly.
            if (pZone->moveFastPC(pSlayer, pSlayer->getX(), pSlayer->getY(), pTargetCreature->getX(),
                                  pTargetCreature->getY(), getSkillType())) {
                decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);

                SkillInput input(pSlayer, pSkillSlot);
                SkillOutput output;
                computeOutput(input, output);

                bool bCriticalHit = false;

                // Deal damage. (There is no skill damage.)
                Damage_t Damage = computeDamage(pSlayer, pTargetCreature, SkillLevel / 5, bCriticalHit);
                setDamage(pTargetCreature, Damage, pSlayer, SkillType, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
                computeAlignmentChange(pTargetCreature, Damage, pSlayer, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
                decreaseDurability(pSlayer, pTargetCreature, pSkillInfo, &_GCSkillToObjectOK1, &_GCSkillToObjectOK2);

                // On a critical hit, knocks the target back.
                if (bCriticalHit) {
                    knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
                }

                if (!pTargetCreature->isSlayer()) {
                    if (bIncreaseDomainExp) {
                        shareAttrExp(pSlayer, Damage, 8, 1, 1, _GCSkillToObjectOK1);
                        increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1,
                                          pTargetCreature->getLevel());
                        increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);
                    }
                    increaseAlignment(pSlayer, pTargetCreature, _GCSkillToObjectOK1);
                }

                // Prepares and sends the packets.
                _GCSkillToObjectOK1.setSkillType(SkillType);
                _GCSkillToObjectOK1.setCEffectID(CEffectID);
                _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
                _GCSkillToObjectOK1.setDuration(0);


                pPlayer->sendPacket(&_GCSkillToObjectOK1);
                _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
                _GCSkillToObjectOK2.setSkillType(SkillType);
                _GCSkillToObjectOK2.setDuration(0);

                if (pTargetCreature->isPC()) {
                    Player* pTargetPlayer = pTargetCreature->getPlayer();
                    Assert(pTargetPlayer != NULL);
                    pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
                } else {
                    Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                    pMonster->addEnemy(pSlayer);
                }

                pSkillSlot->setRunTime(output.Delay);

            } else {
                executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
            }
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

FlashSliding g_FlashSliding;
