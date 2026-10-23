//////////////////////////////////////////////////////////////////////////////
// Filename    : UltimateBlow.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "UltimateBlow.h"

#include "GCAttackArmsOK1.h"
#include "GCAttackArmsOK2.h"
#include "GCAttackArmsOK3.h"
#include "GCAttackArmsOK4.h"
#include "GCAttackArmsOK5.h"
#include "GameContext.h"
#include "ItemUtil.h"

//////////////////////////////////////////////////////////////////////////////
// Slayer object
//////////////////////////////////////////////////////////////////////////////
void UltimateBlow::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, SkillSlot* pSkillSlot, CEffectID_t CEffectID)

{
    __BEGIN_DEBUG


    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        Creature* pTargetCreature = pZone->getCreature(TargetObjectID);

        // A missing target fails the skill instead of throwing.
        if (pTargetCreature == NULL || !canAttack(pSlayer, pTargetCreature) || pTargetCreature->isNPC()) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCAttackArmsOK1 _GCAttackArmsOK1;
        GCAttackArmsOK2 _GCAttackArmsOK2;
        GCAttackArmsOK3 _GCAttackArmsOK3;
        GCAttackArmsOK4 _GCAttackArmsOK4;
        GCAttackArmsOK5 _GCAttackArmsOK5;

        // The skill needs a gun-type weapon in the right hand.
        // SG and SR guns are accepted as well.
        Item* pWeapon = pSlayer->getWearItem(Slayer::WEAR_RIGHTHAND);
        if (pWeapon == NULL || isArmsWeapon(pWeapon) == false)
        //			pWeapon->getItemClass() == Item::ITEM_CLASS_SG ||
        //			pWeapon->getItemClass() == Item::ITEM_CLASS_SR)
        {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        bool bIncreaseExp = pSlayer->isRealWearingEx(Slayer::WEAR_RIGHTHAND);

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();

        SkillInput input(pSlayer, pSkillSlot);
        SkillOutput output;
        computeOutput(input, output);

        // Computes the penalty value.
        int ToHitPenalty = getPercentValue(pSlayer->getToHit(), output.ToHit);

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, output.Range); // pWeapon->getRange());
        bool bBulletCheck = (getRemainBullet(pWeapon) > 0) ? true : false;
        bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature, ToHitPenalty);
        bool bPK = verifyPK(pSlayer, pTargetCreature);

        // The bullet count always drops.
        Bullet_t RemainBullet = 0;
        if (bBulletCheck) {
            // Drops the bullet count and reads back the remaining bullets.
            decreaseBullet(pWeapon);
            // The weapon is not saved on every shot.
            RemainBullet = getRemainBullet(pWeapon);
        }

        if (bManaCheck && bTimeCheck && bRangeCheck && bBulletCheck && bHitRoll && bPK) {
            decreaseMana(pSlayer, RequiredMP, _GCAttackArmsOK1);

            _GCAttackArmsOK5.setSkillSuccess(true);
            _GCAttackArmsOK1.setSkillSuccess(true);


            // Computes the damage and applies the quickfire penalty.
            // output.Damage is negative, so adding its percentage subtracts damage.
            int Damage = max(1, Random(pWeapon->getMinDamage(), pWeapon->getMaxDamage())) *
                         (1.0f + ((float)pSkillSlot->getExpLevel() * 4.0f /
                                  100.f)); // computeDamage(pSlayer, pTargetCreature, SkillLevel/5, bCriticalHit);
            Damage += getPercentValue(Damage, output.Damage);
            Damage = max(0, Damage);


            // Applies the damage.
            setDamage(pTargetCreature, Damage, pSlayer, SkillType, &_GCAttackArmsOK2, &_GCAttackArmsOK1);
            computeAlignmentChange(pTargetCreature, Damage, pSlayer, &_GCAttackArmsOK2, &_GCAttackArmsOK1);

            // A critical hit knocks the target back.

            int dist = 1 + pSkillSlot->getExpLevel() / 50;
            for (int i = 0; i < dist; ++i) {
                knockbackCreature(pSlayer->getZone(), pTargetCreature, pSlayer->getX(), pSlayer->getY());
            }

            if (!pTargetCreature->isSlayer()) {
                if (bIncreaseExp) {
                    shareAttrExp(pSlayer, Damage, 1, 8, 1, _GCAttackArmsOK1);
                    increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCAttackArmsOK1,
                                      pTargetCreature->getLevel());
                    increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCAttackArmsOK1);
                }

                increaseAlignment(pSlayer, pTargetCreature, _GCAttackArmsOK1);
            }
            //}

            _GCAttackArmsOK2.setSkillType(getSkillType());

            if (pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();
                if (pTargetPlayer != NULL) {
                    _GCAttackArmsOK2.setObjectID(pSlayer->getObjectID());
                    pTargetPlayer->sendPacket(&_GCAttackArmsOK2);
                }
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                pMonster->addEnemy(pSlayer);
            }

            // Drop the durability of the attacker's and the target's items.
            decreaseDurability(pSlayer, pTargetCreature, NULL, &_GCAttackArmsOK1, &_GCAttackArmsOK2);

            ZoneCoord_t targetX = pTargetCreature->getX();
            ZoneCoord_t targetY = pTargetCreature->getY();
            ZoneCoord_t myX = pSlayer->getX();
            ZoneCoord_t myY = pSlayer->getY();

            _GCAttackArmsOK1.setSkillType(getSkillType());
            _GCAttackArmsOK1.setObjectID(TargetObjectID);
            _GCAttackArmsOK1.setBulletNum(RemainBullet);

            _GCAttackArmsOK3.setSkillType(getSkillType());
            _GCAttackArmsOK3.setObjectID(pSlayer->getObjectID());
            _GCAttackArmsOK3.setTargetXY(targetX, targetY);

            _GCAttackArmsOK4.setSkillType(getSkillType());
            _GCAttackArmsOK4.setTargetObjectID(TargetObjectID);

            _GCAttackArmsOK5.setSkillType(getSkillType());
            _GCAttackArmsOK5.setObjectID(pSlayer->getObjectID());
            _GCAttackArmsOK5.setTargetObjectID(TargetObjectID);

            pPlayer->sendPacket(&_GCAttackArmsOK1);

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pSlayer);
            cList = pZone->broadcastSkillPacket(myX, myY, targetX, targetY, &_GCAttackArmsOK5, cList);
            pZone->broadcastPacket(myX, myY, &_GCAttackArmsOK3, cList);
            pZone->broadcastPacket(targetX, targetY, &_GCAttackArmsOK4, cList);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            executeSkillFailNormalWithGun(pSlayer, getSkillType(), pTargetCreature, RemainBullet);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_DEBUG
}

UltimateBlow g_UltimateBlow;
