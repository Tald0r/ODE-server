//////////////////////////////////////////////////////////////////////////////
// Filename    : CreateBomb.cpp
// Written by  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "CreateBomb.h"

#include "GCSkillToInventoryOK1.h"
#include "GCSkillToInventoryOK2.h"
#include "GameContext.h"
#include "ItemFactoryManager.h"
#include "ItemUtil.h"
#include "item/Bomb.h"

int MaterialType2BombTypeMap[] = {
    0,  // 0
    1,  // 1
    2,  // 2
    3,  // 3
    4,  // 4
    -1, // 5
    -1, // 6
    -1, // 7
    -1, // 8
    -1  // 9
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CreateBomb::execute(Slayer* pSlayer, ObjectID_t InvenObjectID, CoordInven_t X, CoordInven_t Y,
                         CoordInven_t TargetX, CoordInven_t TargetY, SkillSlot* pSkillSlot)

{
    Assert(pSlayer != NULL);
    Assert(pSkillSlot != NULL);

    try {
        Player* pPlayer = pSlayer->getPlayer();
        Zone* pZone = pSlayer->getZone();
        Inventory* pInventory = pSlayer->getInventory();

        Assert(pPlayer != NULL);
        Assert(pZone != NULL);
        Assert(pInventory != NULL);

        // The item cannot be used if it is null, is not a bomb material,
        // or the object id does not match.
        Item* pBombMaterial = pInventory->getItem(X, Y);
        if (pBombMaterial == NULL || pBombMaterial->getItemClass() != Item::ITEM_CLASS_BOMB_MATERIAL ||
            pBombMaterial->getObjectID() != InvenObjectID) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        bool bSamePosition = false;
        if (X == TargetX && Y == TargetY)
            bSamePosition = true;

        // The source and target positions are the same only when the
        // C4 stack being converted into a bomb holds exactly one item.
        // (The old C4 is deleted and the bomb is created in the same slot.)
        // If the count is not 1, return.
        if (bSamePosition && pBombMaterial->getNum() != 1) {
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        // Get the item type and the bomb type that corresponds to it.
        ItemType_t MaterialType = pBombMaterial->getItemType();
        int BombType = MaterialType2BombTypeMap[MaterialType];
        if (BombType == -1) {
            // A mine material rather than a bomb material fails the skill.
            executeSkillFailException(pSlayer, getSkillType());
            return;
        }

        GCSkillToInventoryOK1 _GCSkillToInventoryOK1;
        GCSkillToInventoryOK2 _GCSkillToInventoryOK2;

        SkillType_t SkillType = pSkillSlot->getSkillType();
        SkillInfo* pSkillInfo = de::gameContext().skillInfos().getSkillInfo(SkillType);
        SkillLevel_t SkillLevel = pSkillSlot->getExpLevel();
        SkillDomainType_t DomainType = pSkillInfo->getDomainType();
        SkillGrade Grade =
            de::gameContext().skillInfos().getGradeByDomainLevel(pSlayer->getSkillDomainLevel(DomainType));

        int RequiredMP = (int)pSkillInfo->getConsumeMP();
        bool bManaCheck = hasEnoughMana(pSlayer, RequiredMP);
        bool bTimeCheck = verifyRunTime(pSkillSlot);
        bool bRangeCheck = checkZoneLevelToUseSkill(pSlayer);
        bool bCanMake = canMake(BombType, pSlayer->getSkillDomainLevel(DomainType), SkillLevel);

        if (bManaCheck && bTimeCheck && bRangeCheck && bCanMake) {
            decreaseMana(pSlayer, RequiredMP, _GCSkillToInventoryOK1);

            SkillInput input(pSlayer, pSkillSlot);
            SkillOutput output;
            computeOutput(input, output);

            list<OptionType_t> optionNULL;
            Item* pBomb = de::gameContext().itemFactories().createItem(Item::ITEM_CLASS_BOMB, BombType, optionNULL);

            // Decrease the bomb material count.
            // Inside this function the material count drops by one automatically,
            // and a last remaining material is deleted from the inventory and the DB.
            decreaseItemNum(pBombMaterial, pInventory, pSlayer->getName(), STORAGE_INVENTORY, 0, X, Y);

            Item* pPrevBomb = pInventory->getItem(TargetX, TargetY);

            // An existing bomb object in the slot means the new one is stacked onto it.
            if (pPrevBomb != NULL) {
                if (canStack(pPrevBomb, pBomb) == false) {
                    // The existing bomb is of a different type; not expected to happen.
                    SAFE_DELETE(pBomb);

                    executeSkillFailException(pSlayer, getSkillType());

                    return;
                }

                // Increase the count by one and save.
                pPrevBomb->setNum(pPrevBomb->getNum() + 1);
                pPrevBomb->save(pSlayer->getName(), STORAGE_INVENTORY, 0, TargetX, TargetY);

                // The decreaseItemNum() call above decreased the item count, so the
                // inventory item count is increased again here.
                pInventory->increaseNum();

                // The newly created bomb was merged into the existing one, so delete it.
                SAFE_DELETE(pBomb);

                _GCSkillToInventoryOK1.setObjectID(pPrevBomb->getObjectID());
            }
            // No existing bomb object means the bomb must be created in the DB.
            else {
                ObjectRegistry& OR = pZone->getObjectRegistry();
                OR.registerObject(pBomb);

                // Put the bomb into the inventory and create it in the DB.
                pBomb->setNum(1);
                pInventory->addItem(TargetX, TargetY, pBomb);
                pBomb->create(pSlayer->getName(), STORAGE_INVENTORY, 0, TargetX, TargetY);

                _GCSkillToInventoryOK1.setObjectID(pBomb->getObjectID());
            }

            // Send the packet.
            _GCSkillToInventoryOK1.setSkillType(SkillType);
            _GCSkillToInventoryOK1.setItemType(BombType);
            _GCSkillToInventoryOK1.setCEffectID(0);
            _GCSkillToInventoryOK1.setX(TargetX);
            _GCSkillToInventoryOK1.setY(TargetY);

            _GCSkillToInventoryOK2.setObjectID(pSlayer->getObjectID());
            _GCSkillToInventoryOK2.setSkillType(SkillType);

            // EXP UP!
            Exp_t ExpUp = 10 * (Grade + 1);
            shareAttrExp(pSlayer, ExpUp, 1, 8, 1, _GCSkillToInventoryOK1);
            increaseDomainExp(pSlayer, DomainType, pSkillInfo->getPoint(), _GCSkillToInventoryOK1);
            increaseSkillExp(pSlayer, DomainType, pSkillSlot, pSkillInfo, _GCSkillToInventoryOK1);

            pPlayer->sendPacket(&_GCSkillToInventoryOK1);

            pZone->broadcastPacket(pSlayer->getX(), pSlayer->getY(), &_GCSkillToInventoryOK2, pSlayer);

            pSkillSlot->setRunTime(output.Delay);
        } else {
            //  Making a bomb has no delay on failure, so the client sends packets in
            //  very rapid succession. Broadcasting the failure packet would make
            //  bystanders see the casting animation repeat extremely fast, as if a
            //  speed hack were in use. So nothing is broadcast here and the packet
            //  goes only to the caster.
            executeSkillFailException(pSlayer, getSkillType());
        }
    } catch (Throwable& t) {
        executeSkillFailException(pSlayer, getSkillType());
    }
}

bool CreateBomb::canMake(ItemType_t BombType, int DomainLevel, int SkillLevel) {
    int ratio = 10 * ((SkillLevel / 10) - BombType) + 40;
    if ((rand() % 100) < ratio)
        return true;
    return false;
}

CreateBomb g_CreateBomb;
