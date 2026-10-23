//////////////////////////////////////////////////////////////////////////////
// Filename    : AbsorbSoul.cpp
// Written by  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "AbsorbSoul.h"

#include "Corpse.h"
#include "GCSkillFailed1.h"
#include "GCSkillToInventoryOK1.h"
#include "GCSkillToTileOK1.h"
#include "GCSkillToTileOK2.h"
#include "GCSkillToTileOK5.h"
#include "GQuestManager.h"
#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "OustersCorpse.h"
#include "PCOustersInfo3.h"
#include "Properties.h"
#include "item/Larva.h"
#include "item/Pupa.h"

//////////////////////////////////////////////////////////////////////////////
// Ousters object handler
//////////////////////////////////////////////////////////////////////////////
// The skill result has to be sent twice:
// one for turning the larva into a pupa,
// and one for the soul absorption itself.
// So when an early condition check fails,
// the SkillFail packet is sent twice.
// ///////////////////////////////////////////////////////////////////////////
void AbsorbSoul::execute(Ousters* pOusters, ObjectID_t TargetObjectID, ZoneCoord_t TargetZoneX, ZoneCoord_t TargetZoneY,
                         ObjectID_t ItemObjectID, CoordInven_t InvenX, CoordInven_t InvenY, CoordInven_t TargetInvenX,
                         CoordInven_t TargetInvenY)

{
    Assert(pOusters != NULL);
    // When the client is locked, the verification packet has to be sent twice.
    bool bClientLocked = InvenX != 255;

    try {
        Player* pPlayer = pOusters->getPlayer();
        Zone* pZone = pOusters->getZone();
        Assert(pPlayer != NULL);
        Assert(pZone != NULL);

        // The corpse may still be a Creature, or it may already be an Item.
        Item* pTargetItem = pZone->getItem(TargetObjectID);
        Creature* pTargetCreature = NULL;

        if (pTargetItem == NULL) {
            pTargetCreature = pZone->getCreature(TargetObjectID);
        }

        if (pTargetCreature == NULL && pTargetItem == NULL) {
            executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
            return;
        }
        // An NPC cannot be attacked.
        // Invulnerability check.
        // A creature that is not in a coma cannot have its soul absorbed.
        if (pTargetCreature != NULL) {
            if (pTargetCreature->isNPC() || !canAttack(pOusters, pTargetCreature) ||
                !pTargetCreature->isFlag(Effect::EFFECT_CLASS_COMA)) {
                executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
                return;
            }
            if (pTargetCreature->isFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL)) {
                //
                executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, true, bClientLocked);
                return;
            }
        } else if (pTargetItem != NULL) {
            if (pTargetItem->getItemClass() != Item::ITEM_CLASS_CORPSE) {
                executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
                return;
            }
            if (pTargetItem->isFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL)) {
                executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, true, bClientLocked);
                return;
            }
        } else {
            executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
            return;
        }

        GCSkillToTileOK1 _GCSkillToTileOK1;
        GCSkillToTileOK2 _GCSkillToTileOK2;
        GCSkillToTileOK5 _GCSkillToTileOK5;


        // Soul absorption on another race's corpse, while the corpse is still a Creature.
        if (pTargetCreature != NULL) //&& bRangeCheck)
        {
            int targetLevel = 0;
            if (pTargetCreature->isSlayer()) {
                Slayer* pTargetSlayer = dynamic_cast<Slayer*>(pTargetCreature);
                targetLevel = pTargetSlayer->getHighestSkillDomainLevel();
            } else if (pTargetCreature->isVampire()) {
                Vampire* pTargetVampire = dynamic_cast<Vampire*>(pTargetCreature);
                targetLevel = pTargetVampire->getLevel();
            } else if (pTargetCreature->isOusters()) {
                Ousters* pTargetOusters = dynamic_cast<Ousters*>(pTargetCreature);
                targetLevel = pTargetOusters->getLevel();
            } else if (pTargetCreature->isMonster()) {
                Monster* pMonster = dynamic_cast<Monster*>(pTargetCreature);
                targetLevel = pMonster->getLevel();
            }

            // The pupa creation has to be handled first.
            // The larva is turned into a pupa.
            // That is done by a separate function below.
            // The item packets must go out before SkillOK, without exception.
            if (InvenX != 255)
                makeLarvaToPupa(pOusters, targetLevel, ItemObjectID, InvenX, InvenY, TargetInvenX, TargetInvenY);

            // Soul absorption itself grants no experience.

            // Absorbing a soul raises the absorber's MP.
            // The gain is proportional to the creature's experience value.
            MP_t CurrentMP = pOusters->getMP();
            MP_t MaxMP = pOusters->getMP(ATTR_MAX);
            MP_t HealPoint = 0;

            if (CurrentMP < MaxMP) {
                HealPoint = computeCreatureExp(pTargetCreature, 60);
            } else {
                float ExtraMP = (float)(CurrentMP - MaxMP) / (float)(MaxMP * 2) * 100;
                float ftemp = 1.1 - ((float)ExtraMP / ((float)ExtraMP + 10.0));
                int ratio = (int)(ftemp * 100);
                HealPoint = computeCreatureExp(pTargetCreature, ratio);
            }

            MP_t NewMP = min((int)MaxMP * 3, (int)CurrentMP + (int)HealPoint);

            // Set the Ousters' MP.
            pOusters->setMP(NewMP);

            GCModifyInformation gcMI;
            gcMI.addShortData(MODIFY_CURRENT_MP, pOusters->getMP(ATTR_CURRENT));


            pOusters->getPlayer()->sendPacket(&gcMI);

            _GCSkillToTileOK1.setSkillType(getSkillType());
            _GCSkillToTileOK1.addCListElement(TargetObjectID);
            _GCSkillToTileOK1.setDuration(0);
            _GCSkillToTileOK1.setX(TargetZoneX);
            _GCSkillToTileOK1.setY(TargetZoneY);
            _GCSkillToTileOK1.setRange(0);

            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.addCListElement(TargetObjectID);
            _GCSkillToTileOK5.setX(TargetZoneX);
            _GCSkillToTileOK5.setY(TargetZoneY);
            _GCSkillToTileOK5.setRange(0);
            _GCSkillToTileOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            if (pTargetCreature != NULL && pTargetCreature->isPC()) {
                Player* pTargetPlayer = pTargetCreature->getPlayer();

                if (pTargetPlayer != NULL) {
                    _GCSkillToTileOK2.setSkillType(getSkillType());
                    _GCSkillToTileOK2.setObjectID(pOusters->getObjectID());
                    _GCSkillToTileOK2.addCListElement(TargetObjectID);
                    _GCSkillToTileOK2.setX(TargetZoneX);
                    _GCSkillToTileOK2.setY(TargetZoneY);
                    _GCSkillToTileOK2.setRange(0);
                    _GCSkillToTileOK2.setDuration(0);

                    pTargetPlayer->sendPacket(&_GCSkillToTileOK2);
                }
            }

            list<Creature*> cList;
            cList.push_back(pTargetCreature);
            cList.push_back(pOusters);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCSkillToTileOK5, cList);

            // Once drained, the target cannot be drained again,
            // and it cannot be resurrected afterwards.
            pTargetCreature->setFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL);

            pOusters->getGQuestManager()->blooddrain();
        } else if (pTargetItem != NULL) //&& bRangeCheck)
        {
            Corpse* pTargetCorpse = dynamic_cast<Corpse*>(pTargetItem);

            int targetLevel = 0;
            Exp_t Exp = 0;

            if (pTargetCorpse->getItemType() == OUSTERS_CORPSE) {
                OustersCorpse* pOustersCorpse = dynamic_cast<OustersCorpse*>(pTargetCorpse);
                Assert(pOustersCorpse != NULL);

                if (pOustersCorpse->getOustersInfo().getName() == pOusters->getName()) {
                    executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, true, bClientLocked);
                    return;
                }
            }

            if (pTargetCorpse->getItemType() == MONSTER_CORPSE || pTargetCorpse->getItemType() == SLAYER_CORPSE ||
                pTargetCorpse->getItemType() == VAMPIRE_CORPSE || pTargetCorpse->getItemType() == OUSTERS_CORPSE) {
                targetLevel = (int)(pTargetCorpse->getLevel());
                Exp = (Exp_t)(pTargetCorpse->getExp());
            } else {
                executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
                return;
            }

            // The pupa creation has to be handled first.
            // The larva is turned into a pupa.
            // That is done by a separate function below.
            // The item packets must go out before SkillOK, without exception.
            if (bClientLocked)
                makeLarvaToPupa(pOusters, targetLevel, ItemObjectID, InvenX, InvenY, TargetInvenX, TargetInvenY);

            // Absorbing a soul raises the absorber's MP.
            // The gain is a percentage of the corpse's experience value,
            // and that percentage falls off once MP is already above the maximum.
            MP_t CurrentMP = pOusters->getMP();
            MP_t MaxMP = pOusters->getMP(ATTR_MAX);
            MP_t HealPoint = 0;

            if (CurrentMP < MaxMP) {
                HealPoint = getPercentValue(Exp, 60);
            } else {
                float ExtraMP = (float)(CurrentMP - MaxMP) / (float)(MaxMP * 2) * 100;
                float ftemp = 1.1 - ((float)ExtraMP / ((float)ExtraMP + 10.0));
                int ratio = (int)(ftemp * 100);
                HealPoint = getPercentValue(Exp, ratio);
            }

            MP_t NewMP = min((int)MaxMP * 3, (int)CurrentMP + (int)HealPoint);

            // Set the Ousters' MP.
            pOusters->setMP(NewMP);

            GCModifyInformation gcMI;
            gcMI.addShortData(MODIFY_CURRENT_MP, pOusters->getMP(ATTR_CURRENT));


            pOusters->getPlayer()->sendPacket(&gcMI);

            _GCSkillToTileOK1.setSkillType(getSkillType());
            _GCSkillToTileOK1.addCListElement(TargetObjectID);
            _GCSkillToTileOK1.setDuration(0);
            _GCSkillToTileOK1.setX(TargetZoneX);
            _GCSkillToTileOK1.setY(TargetZoneY);
            _GCSkillToTileOK1.setRange(0);

            _GCSkillToTileOK5.setSkillType(getSkillType());
            _GCSkillToTileOK5.setObjectID(pOusters->getObjectID());
            _GCSkillToTileOK5.addCListElement(TargetObjectID);
            _GCSkillToTileOK5.setX(TargetZoneX);
            _GCSkillToTileOK5.setY(TargetZoneY);
            _GCSkillToTileOK5.setRange(0);
            _GCSkillToTileOK5.setDuration(0);

            pPlayer->sendPacket(&_GCSkillToTileOK1);

            list<Creature*> cList;
            cList.push_back(pOusters);
            pZone->broadcastPacket(pOusters->getX(), pOusters->getY(), &_GCSkillToTileOK5, cList);

            // Once drained, the item cannot be drained again.
            pTargetItem->setFlag(Effect::EFFECT_CLASS_CANNOT_ABSORB_SOUL);

            pOusters->getGQuestManager()->blooddrain();
        } else {
            executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
        }
    } catch (Throwable& t) {
        executeAbsorbSoulSkillFail(pOusters, getSkillType(), TargetObjectID, false, bClientLocked);
    }
}

void AbsorbSoul::makeLarvaToPupa(Ousters* pOusters, int TargetLevel, ObjectID_t ItemObjectID, CoordInven_t InvenX,
                                 CoordInven_t InvenY, CoordInven_t TargetInvenX, CoordInven_t TargetInvenY) {
    Inventory* pInventory = pOusters->getInventory();
    Assert(pInventory != NULL);
    Zone* pZone = pOusters->getZone();
    Assert(pZone != NULL);

    if (InvenX >= pInventory->getWidth() || InvenY >= pInventory->getHeight() ||
        TargetInvenX >= pInventory->getWidth() || TargetInvenY >= pInventory->getHeight()) {
        executeSkillFailException(pOusters, getSkillType());
        return;
    }

    Item* pLarva = pInventory->getItem(InvenX, InvenY);
    if (pLarva == NULL || pLarva->getItemClass() != Item::ITEM_CLASS_LARVA || pLarva->getObjectID() != ItemObjectID) {
        executeSkillFailException(pOusters, getSkillType());
        return;
    }

    ItemType_t LarvaType = pLarva->getItemType();

    // The chance is quadrupled.
    int ratio = (200 * TargetLevel) / (pOusters->getLevel() * (pLarva->getItemType() + 1));

    // Pupa creation failed.
    if ((rand() % 100) > ratio) {
        executeSkillFailException(pOusters, getSkillType());
        return;
    }

    // The roll succeeded, so build the pupa.
    list<OptionType_t> optionNULL;
    Item* pPupa =
        de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_PUPA, pLarva->getItemType(), optionNULL);

    // Decrease the larva count.
    // The call drops the count by one and
    // deletes the larva from the inventory and the database if it was the last one.
    decreaseItemNum(pLarva, pInventory, pOusters->getName(), STORAGE_INVENTORY, 0, InvenX, InvenY);

    // Fetch the pupa already sitting in the target slot.
    Item* pPrevPupa = pInventory->getItem(TargetInvenX, TargetInvenY);

    GCSkillToInventoryOK1 gcSkillToInventoryOK1;

    if (pPrevPupa != NULL) {
        // There is already a pupa to stack onto.

        if (!canStack(pPrevPupa, pPupa) || pPrevPupa->getNum() >= ItemMaxStack[(int)pPrevPupa->getItemClass()]) {
            executeSkillFailException(pOusters, getSkillType());
            return;
        }

        // Increase the count by one and save it.
        pPrevPupa->setNum(pPrevPupa->getNum() + 1);
        pPrevPupa->save(pOusters->getName(), STORAGE_INVENTORY, 0, TargetInvenX, TargetInvenY);

        // decreaseItemNum() above dropped the inventory's item count,
        // so raise it back here.
        pInventory->increaseNum();

        // The new pupa was merged into the existing one, so delete it.
        SAFE_DELETE(pPupa);

        gcSkillToInventoryOK1.setObjectID(pPrevPupa->getObjectID());
    } else {
        ObjectRegistry& OR = pZone->getObjectRegistry();
        OR.registerObject(pPupa);

        // Put the pupa in the inventory and create it in the database.
        pInventory->addItem(TargetInvenX, TargetInvenY, pPupa);
        pPupa->create(pOusters->getName(), STORAGE_INVENTORY, 0, TargetInvenX, TargetInvenY);

        gcSkillToInventoryOK1.setObjectID(pPupa->getObjectID());
    }

    gcSkillToInventoryOK1.setSkillType(getSkillType());
    gcSkillToInventoryOK1.setItemType(LarvaType);
    gcSkillToInventoryOK1.setCEffectID(0);
    gcSkillToInventoryOK1.setX(TargetInvenX);
    gcSkillToInventoryOK1.setY(TargetInvenY);

    pOusters->getPlayer()->sendPacket(&gcSkillToInventoryOK1);
}

AbsorbSoul g_AbsorbSoul;
