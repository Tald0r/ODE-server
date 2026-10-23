//////////////////////////////////////////////////////////////////////////////
// Filename    : ThrowHolyWater.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "ThrowHolyWater.h"

#include "GCModifyInformation.h"
#include "GCSkillToObjectOK1.h"
#include "GCThrowItemOK1.h"
#include "GCThrowItemOK2.h"
#include "GCThrowItemOK3.h"
#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "item/HolyWater.h"


//////////////////////////////////////////////////////////////////////
//
// ThrowHolyWater::execute()
//
//////////////////////////////////////////////////////////////////////
void ThrowHolyWater::execute(Slayer* pSlayer, ObjectID_t TargetObjectID, ObjectID_t ItemObjectID, CoordInven_t InvenX,
                             CoordInven_t InvenY)

{
    Assert(pSlayer != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Inventory* pInventory = pSlayer->getInventory();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);
        Assert(pInventory != NULL);
        Assert(InvenX < pInventory->getWidth());
        Assert(InvenY < pInventory->getHeight());

        Item* pItem = pInventory->getItem(InvenX, InvenY);

        if (pItem == NULL) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        ObjectID_t ObjectID = pItem->getObjectID();
        Creature* pTargetCreature = NULL;

        // If the item id does not match the one in the packet, or the item is not
        // holy water, report failure.
        if (ObjectID != ItemObjectID || pItem->getItemClass() != Item::ITEM_CLASS_HOLYWATER) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToObjectOK1 _GCSkillToObjectOK1;
        GCThrowItemOK2 _GCThrowItemOK2;
        GCModifyInformation gcAttackerMI;

        // Find the target creature in the zone.

        pTargetCreature = pZone->getCreature(TargetObjectID);

        // The pointer can be NULL because of synchronization with the client:
        // when two holy water bottles are thrown in a row and the first one kills
        // the target, the client may not have received that packet yet and throws
        // again, so simply returning is not enough.
        if (pTargetCreature == NULL) {
            // The item count must be decreased first.
            decreaseItemNum(pItem, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
            executeSkillFailException(pSlayer, getSkillType());
            return;
        } else {
            HolyWater* pHolyWater = dynamic_cast<HolyWater*>(pItem);

            Damage_t MinDamage = pHolyWater->getMinDamage();
            Damage_t MaxDamage = pHolyWater->getMaxDamage();
            Damage_t Damage = max(1, Random(MinDamage, MaxDamage));

            // Damage does not apply outside normal areas.
            // checkZoneLevelToHitTarget() is called below, so no safe zone check is
            // needed here.

            list<Creature*> cList;
            cList.push_back(pSlayer);

            bool bHitRoll = HitRoll::isSuccess(pSlayer, pTargetCreature);
            bool bPK = verifyPK(pSlayer, pTargetCreature);
            bool bRangeCheck = verifyDistance(pSlayer, pTargetCreature, 10);
            bool bZoneLevelCheck = checkZoneLevelToHitTarget(pTargetCreature) && canAttack(pSlayer, pTargetCreature);

            // Deal damage on a hit.
            // The holy water is spent even on a miss.
            if (bHitRoll && bPK && bRangeCheck && bZoneLevelCheck) {
                // Experience is granted only when the target is not a Slayer.
                if (!pTargetCreature->isSlayer()) {
                    shareAttrExp(pSlayer, Damage, 1, 1, 8, gcAttackerMI);

                    // The enchant domain experience is raised too.
                    // 2003. 1. 12 by bezz
                    // There is no SkillInfo for Throw Holy Water, so the point value of
                    // Create Holy Water is used.
                    SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SKILL_CREATE_HOLY_WATER);
                    increaseDomainExp(pSlayer, SKILL_DOMAIN_ENCHANT, pSkillInfo->getPoint(), gcAttackerMI,
                                      pTargetCreature->getLevel());
                }

                if (pTargetCreature->isSlayer()) {
                } else if (pTargetCreature->isVampire()) {
                    Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetCreature);

                    setDamage(pTargetVampire, Damage, NULL, 0, &_GCThrowItemOK2);

                    // Add 10% of the damage as silver damage.
                    Silver_t silverDamage = max(1, getPercentValue(Damage, 10));
                    Silver_t newSilverDamage = pTargetVampire->getSilverDamage() + silverDamage;
                    pTargetVampire->saveSilverDamage(newSilverDamage);

                    Player* pTargetPlayer = pTargetVampire->getPlayer();
                    pTargetPlayer->sendPacket(&_GCThrowItemOK2);

                    GCModifyInformation gcModifyInfo;
                    gcModifyInfo.addShortData(MODIFY_SILVER_DAMAGE, newSilverDamage);
                    pTargetPlayer->sendPacket(&gcModifyInfo);
                } else if (pTargetCreature->isOusters()) {
                    Ousters* pTargetOusters = dynamic_cast<Ousters*>(pTargetCreature);

                    setDamage(pTargetOusters, Damage, NULL, 0, &_GCThrowItemOK2);

                    // Add 10% of the damage as silver damage.
                    Silver_t silverDamage = max(1, getPercentValue(Damage, 10));
                    Silver_t newSilverDamage = pTargetOusters->getSilverDamage() + silverDamage;
                    pTargetOusters->saveSilverDamage(newSilverDamage);

                    Player* pTargetPlayer = pTargetOusters->getPlayer();
                    pTargetPlayer->sendPacket(&_GCThrowItemOK2);

                    GCModifyInformation gcModifyInfo;
                    gcModifyInfo.addShortData(MODIFY_SILVER_DAMAGE, newSilverDamage);
                    pTargetPlayer->sendPacket(&gcModifyInfo);
                } else if (pTargetCreature->isMonster()) {
                    Monster* pTargetMonster = dynamic_cast<Monster*>(pTargetCreature);
                    setDamage(pTargetMonster, Damage, pSlayer, getSkillType(), NULL, NULL);

                    Silver_t silverDamage = max(1, getPercentValue(Damage, 10));
                    Silver_t newSilverDamage = pTargetMonster->getSilverDamage() + silverDamage;
                    pTargetMonster->setSilverDamage(newSilverDamage);
                }

                cList.push_back(pTargetCreature);

                // Send the OK3 packet to the people nearby.
                GCThrowItemOK3 _GCThrowItemOK3;
                _GCThrowItemOK3.setObjectID(pSlayer->getObjectID());
                _GCThrowItemOK3.setTargetObjectID(TargetObjectID);
                pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCThrowItemOK3, cList);

                // Send the OK packet to the thrower.
                GCThrowItemOK1 _GCThrowItemOK1;
                _GCThrowItemOK1.setObjectID(TargetObjectID);

                pPlayer->sendPacket(&_GCThrowItemOK1);
                pPlayer->sendPacket(&gcAttackerMI);
            } else // The holy water throw failed.
            {
                executeSkillFailNormal(pSlayer, getSkillType(), pTargetCreature);
            }

            // The holy water count is decreased whether or not the throw hit.
            decreaseItemNum(pItem, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

ThrowHolyWater g_ThrowHolyWater;
