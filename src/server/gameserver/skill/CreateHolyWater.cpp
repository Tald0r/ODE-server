//////////////////////////////////////////////////////////////////////////////
// Filename    : CreateHolyWater.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CreateHolyWater.h"

#include "GCSkillToInventoryOK1.h"
#include "GCSkillToInventoryOK2.h"
#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "item/HolyWater.h"


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CreateHolyWater::execute(Slayer* pSlayer, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
                              CoordInven_t TargetX, CoordInven_t TargetY, SkillSlot* pSkillSlot)

{
    __BEGIN_TRY


    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Inventory* pInventory = pSlayer->getInventory();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);
        Assert(pInventory != NULL);

        // The item cannot be used if it is null, is not plain water,
        // or the object id does not match.
        Item* pWater = pInventory->getItem(X, Y);
        if (pWater == NULL || pWater->getItemClass() != Item::ITEM_CLASS_WATER ||
            pWater->getObjectID() != InvenObjectID) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // The source and target positions are the same only when the
        // bottle stack being converted into holy water holds exactly one item.
        // (The old bottle is deleted and the holy water is created in the same slot.)
        // If the count is not 1, return.
        // The whole stack converts at once, so this count check is not needed.

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;
        GCSkillToInventoryOK2 _GCSkillToInventoryOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillGrade Grade =
            de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));

        ItemType_t waterType = pWater->getItemType();

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bCanMake = canMake(waterType, pSlayer->getSkillDomainLevel(DomainType), SkillLevel);

        if (bManaCheck && bTimeCheck && bRangeCheck && bCanMake) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            // Create the holy water from the plain water's item type.
            // This works because the Water and HolyWater item types correspond
            // one to one.
            list<OptionType_t> optionNULL;
            Item* pHolyWater =
                de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_HOLYWATER, waterType, optionNULL);
            // Sets the count as well.
            pHolyWater->setNum(pWater->getNum());

            // Decrease the bottle count.
            // Inside this function the bottle count drops by one automatically,
            // and a last remaining bottle is deleted from the inventory and the DB.
            // Converts the whole stack, however many there are.
            pInventory->deleteItem(X, Y);
            pWater->destroy();
            SAFE_DELETE(pWater);

            Item* pPrevHolyWater = pInventory->getItem(TargetX, TargetY);

            // An existing holy water object in the slot means the new one is stacked onto it.
            if (pPrevHolyWater != NULL) {
                if (canStack(pPrevHolyWater, pHolyWater) == false) {
                    // The existing holy water is of a different type; not expected to happen.
                    SAFE_DELETE(pHolyWater);

                    executeSkillFailException(pSlayer, getSkillType());

                    return;
                }

                // Increase the count by one and save.
                pPrevHolyWater->setNum(pPrevHolyWater->getNum() + pHolyWater->getNum());
                pPrevHolyWater->save(pSlayer->getName(), STORAGE_INVENTORY, 0, TargetX, TargetY);

                // The decreaseItemNum() call above decreased the item count, so the
                // inventory item count is increased again here.
                pInventory->increaseNum(pHolyWater->getNum());

                // The newly created holy water was merged into the existing one, so delete it.
                SAFE_DELETE(pHolyWater);

                _GCSkillToInventoryOK1.setObjectID(pPrevHolyWater->getObjectID());
            }
            // No existing holy water object means it must be created in the DB.
            else {
                ObjectRegistry& OR = pZone->getObjectRegistry();
                OR.registerObject(pHolyWater);

                // Put the holy water into the inventory and create it in the DB.
                pInventory->addItem(TargetX, TargetY, pHolyWater);
                pHolyWater->create(pSlayer->getName(), STORAGE_INVENTORY, 0, TargetX, TargetY);

                _GCSkillToInventoryOK1.setObjectID(pHolyWater->getObjectID());
            }

            // Send the packet.
            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setItemType(waterType);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setX(TargetX);
            _GCSkillToInventoryOK1.setY(TargetY);

            _GCSkillToInventoryOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToInventoryOK2.setSkillType(SkillType);

            // EXP UP!
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 1, 8, _GCSkillToInventoryOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToInventoryOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToInventoryOK1);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToInventoryOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            //  Making holy water has no delay on failure, so the client sends packets in
            //  very rapid succession. Broadcasting the failure packet would make
            //  bystanders see the casting animation repeat extremely fast, as if a
            //  speed hack were in use. So nothing is broadcast here and the packet
            //  goes only to the caster.
            executeSkillFailException(pSlayer, getSkillType());
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }


    __END_CATCH
}

bool CreateHolyWater::canMake(ItemType_t WaterType, int DomainLevel, int SkillLevel) {
    __BEGIN_TRY

    bool rvalue = false;
    SkillGrade Grade = de::gameContext().skillInfos().getGradeByDomainLevel(DomainLevel);
    int ratio = 0;

    // The domain level limits the size of holy water that can be made.
    // A domain level of master or above should carry no penalty, but that is
    // not handled.
    if (WaterType == 2 && Grade >= SKILL_GRADE_EXPERT) {
        // Expert or above is required to make large holy water.
        rvalue = true;
        ratio = 50 + SkillLevel - 50;
    } else if (WaterType == 1 && Grade >= SKILL_GRADE_ADEPT) {
        // Adept or above is required to make medium holy water.
        rvalue = true;
        ratio = 50 + SkillLevel - 30;
    } else if (WaterType == 0 && Grade >= SKILL_GRADE_APPRENTICE) {
        // Apprentice or above is required to make small holy water.
        rvalue = true;
        ratio = 50 + SkillLevel - 10;
    }

    // If it can be made at all, roll the chance.
    if (rvalue) {
        if ((rand() % 100) < ratio)
            return true;
    }

    return false;

    __END_CATCH
}

CreateHolyWater g_CreateHolyWater;
