//////////////////////////////////////////////////////////////////////////////
// Filename    : LarSlash.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "LarSlash.h"

#include "CrossCounter.h"
#include "GCSkillToObjectOK1.h"
#include "GCSkillToObjectOK2.h"
#include "GCSkillToObjectOK5.h"
#include "GameContext.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object handler
//////////////////////////////////////////////////////////////////////////////
void LarSlash::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // An NPC cannot be attacked.
        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // The skill needs a sword in the right hand.
        Item* pItem = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pItem == NULL || pItem->getItemClass() != Item::ITEM_CLASS_SWORD) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        bool bIncreaseExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCSkillToObjectOK2 _GCSkillToObjectOK2;
        GCSkillToObjectOK5 _GCSkillToObjectOK5;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();

        bool bCriticalHit = false;

        // Adds the skill damage to the base damage.
        SkillInput input(pSlayer, pSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        Damage_t Damage = computeDamage(pSlayer, pTargetCreature, SkillLevel / 5, bCriticalHit);
        Damage += output.Damage;

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, pSkillInfo->getRange());
        bool bHitRoll = (rand() % 10) ? HitRoll::isSuccess(pSlayer, pTargetCreature, SkillLevel / 2) : (true);
        bool bCanHit = canHit(pSlayer, pTargetCreature, SkillType);
        bool bPK = verifyPK(pSlayer, pTargetCreature);

        // Succeeds when there is enough mana, the time and range checks pass,
        // the hit roll succeeds and no cross counter is active.
        if (bManaCheck && bTimeCheck && bRangeCheck && bHitRoll && bCanHit && bPK) {
            CheckCrossCounter(pSlayer, pTargetCreature, Damage, pSkillInfo->getRange());

            decreaseMana(pSlayer, RequiredMP, _GCSkillToObjectOK1);


            // Apply the damage and reduce durability.
            setDamage(pTargetCreature, Damage, pSlayer, SkillType, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
            computeAlignmentChange(pTargetCreature, Damage, pSlayer, &_GCSkillToObjectOK2, &_GCSkillToObjectOK1);
            decreaseDurability(pSlayer, pTargetCreature, pSkillInfo, &_GCSkillToObjectOK1, &_GCSkillToObjectOK2);

            // A critical hit knocks the target back.
            if (bCriticalHit) {
                knockbackCreature(pZone, pTargetCreature, pSlayer->getX(), pSlayer->getY());
            }

            // Experience is raised only when the target is not a Slayer.
            if (!pTargetCreature->isSlayer()) {
                if (bIncreaseExp) {
                    shareAttrExp(pSlayer, Damage, 8, 1, 1, _GCSkillToObjectOK1);
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToObjectOK1,
                                      pTargetCreature->getLevel());
                    increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToObjectOK1);
                }
                increaseAlignment(pSlayer, pTargetCreature, _GCSkillToObjectOK1);
            }

            // Prepares the packet and sends it.
            _GCSkillToObjectOK1.setSkillType(SkillType);
            _GCSkillToObjectOK1.setCEffectID(CEffectID);
            _GCSkillToObjectOK1.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK1.setDuration(0);

            _GCSkillToObjectOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK2.setSkillType(SkillType);
            _GCSkillToObjectOK2.setDuration(0);

            _GCSkillToObjectOK5.setObjectID(pSlayer->getObjectID());
            _GCSkillToObjectOK5.setSkillType(SkillType);
            _GCSkillToObjectOK5.setTargetObjectID(TargetObjectID);
            _GCSkillToObjectOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToObjectOK1);

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                Assert(pTargetPlayer != NULL);
                pTargetPlayer->sendPacket(&_GCSkillToObjectOK2);
            } else {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pSlayer);
            }

            list<Creature*> cList;
            cList.push_back(pSlayer);
            cList.push_back(pTargetCreature);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToObjectOK5, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

LarSlash g_LarSlash;
