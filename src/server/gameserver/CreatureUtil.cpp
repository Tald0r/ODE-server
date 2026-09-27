////////////////////////////////////////////////////////////////////////////////
// Filename : CreatureUtil.cpp
// Description :
// Code common to the Slayer and Vampire files, factored out to keep the size of
// those two files down as far as possible.
// Once a PlayerCreature class exists, the contents of this file should be moved
// into it.
////////////////////////////////////////////////////////////////////////////////

#include "CreatureUtil.h"

#include <cstdio>

#include "Belt.h"
#include "BloodBible.h"
#include "CastleInfoManager.h"
#include "CombatInfoManager.h"
#include "Effect.h"
#include "EffectAftermath.h"
#include "EffectGnomesWhisper.h"
#include "EffectGrandMasterSlayer.h"
#include "EffectGrandMasterVampire.h"
#include "EffectKillAftermath.h"
#include "EffectObservingEye.h"
#include "EffectRelicLock.h"
#include "FlagSet.h"
#include "GCAddEffect.h"
#include "GCAddSlayer.h"
#include "GCAddVampire.h"
#include "GCDeleteInventoryItem.h"
#include "GCDeleteObject.h"
#include "GCNoticeEvent.h"
#include "GCNotifyWin.h"
#include "GCPetInfo.h"
#include "GCSystemMessage.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "GameWorldInfoManager.h"
#include "Inventory.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "LevelWarManager.h"
#include "Monster.h"
#include "MonsterInfo.h"
#include "Ousters.h"
#include "PacketUtil.h"
#include "PetInfo.h"
#include "Player.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "Relic.h"
#include "RelicUtil.h"
#include "ShrineInfoManager.h"
#include "SimpleCreatureEffect.h"
#include "Slayer.h"
#include "Stash.h"
#include "Store.h"
#include "Tile.h"
#include "TimeChecker.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "Zone.h"
#include "ZoneGroupManager.h"
#include "couple/PartnerWaitingManager.h"
#include "ctf/FlagManager.h"
#include "gm/GMCommands.h"
#include "repository/CharacterPurgeRepository.h"
#include "repository/CharacterRepository.h"
#include "repository/PlayRecordRepository.h"
#include "skill/Sniping.h"
#include "skill/SummonGroundElemental.h"

////////////////////////////////////////////////////////////////////////////////
// Are the two creatures of the same creature class?
////////////////////////////////////////////////////////////////////////////////
bool isSameRace(Creature* pCreature1, Creature* pCreature2)

{
    __BEGIN_TRY

    Assert(pCreature1 != NULL);
    Assert(pCreature2 != NULL);

    return (pCreature1->getCreatureClass() == pCreature2->getCreatureClass() ? true : false);

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// Find an item by ObjectID
//
////////////////////////////////////////////////////////////////////////////////
Item* findItemOID(Creature* pCreature, ObjectID_t id)

{
    __BEGIN_TRY

    int storage, x, y;
    return findItemOID(pCreature, id, storage, x, y);

    __END_CATCH
}

Item* findItemOID(Creature* pCreature, ObjectID_t id, Item::ItemClass IClass)

{
    __BEGIN_TRY

    int storage, x, y;
    return findItemOID(pCreature, id, IClass, storage, x, y);

    __END_CATCH
}

Item* findItemOID(Creature* pCreature, ObjectID_t id, int& storage, int& x, int& y)

{
    __BEGIN_TRY

    if (pCreature == NULL)
        return NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Stash* pStash = pPC->getStash();
    Item* pItem = NULL;
    CoordInven_t tx = 0;
    CoordInven_t ty = 0;

    // Search the inventory.
    pItem = pInventory->findItemOID(id, tx, ty);
    if (pItem != NULL) {
        storage = STORAGE_INVENTORY;
        x = tx;
        y = ty;
        return pItem;
    }

    // Search the gear slots.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    }

    // Search the mouse slot.
    pItem = pPC->getExtraInventorySlotItem();
    if (pItem != NULL && pItem->getObjectID() == id) {
        storage = STORAGE_EXTRASLOT;
        return pItem;
    }

    // Search the stash.
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            pItem = pStash->get(r, i);
            if (pItem != NULL && pItem->getObjectID() == id) {
                storage = STORAGE_STASH;
                x = r;
                y = i;
                return pItem;
            }
        }
    }

    return NULL;

    __END_CATCH
}

Item* findItemOID(Creature* pCreature, ObjectID_t id, Item::ItemClass IClass, int& storage, int& x, int& y)

{
    __BEGIN_TRY

    if (pCreature == NULL)
        return NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Stash* pStash = pPC->getStash();
    Item* pItem = NULL;
    CoordInven_t tx = 0;
    CoordInven_t ty = 0;

    // Search the inventory.
    pItem = pInventory->findItemOID(id, IClass, tx, ty);
    if (pItem != NULL) {
        storage = STORAGE_INVENTORY;
        x = tx;
        y = ty;
        return pItem;
    }

    // Search the gear slots.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL && pItem->getObjectID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    }

    // Search the mouse slot.
    pItem = pPC->getExtraInventorySlotItem();
    if (pItem != NULL && pItem->getObjectID() == id && pItem->getItemClass() == IClass) {
        storage = STORAGE_EXTRASLOT;
        return pItem;
    }

    // Search the stash.
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            pItem = pStash->get(r, i);
            if (pItem != NULL && pItem->getObjectID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_STASH;
                x = r;
                y = i;
                return pItem;
            }
        }
    }

    return NULL;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
//
// Find an item by ItemID
//
////////////////////////////////////////////////////////////////////////////////

Item* findItemIID(Creature* pCreature, ItemID_t id)

{
    __BEGIN_TRY

    int storage, x, y;
    return findItemIID(pCreature, id, storage, x, y);

    __END_CATCH
}

Item* findItemIID(Creature* pCreature, ItemID_t id, Item::ItemClass IClass)

{
    __BEGIN_TRY

    int storage, x, y;
    return findItemIID(pCreature, id, IClass, storage, x, y);

    __END_CATCH
}

Item* findItemIID(Creature* pCreature, ItemID_t id, int& storage, int& x, int& y)

{
    __BEGIN_TRY

    if (pCreature == NULL)
        return NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Stash* pStash = pPC->getStash();
    Item* pItem = NULL;
    CoordInven_t tx = 0;
    CoordInven_t ty = 0;

    // Search the inventory.
    pItem = pInventory->findItemIID(id, tx, ty);
    if (pItem != NULL) {
        storage = STORAGE_INVENTORY;
        x = tx;
        y = ty;
        return pItem;
    }

    // Search the gear slots.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    }

    // Search the mouse slot.
    pItem = pPC->getExtraInventorySlotItem();
    if (pItem != NULL && pItem->getItemID() == id) {
        storage = STORAGE_EXTRASLOT;
        return pItem;
    }

    // Search the stash.
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            pItem = pStash->get(r, i);
            if (pItem != NULL && pItem->getItemID() == id) {
                storage = STORAGE_STASH;
                x = r;
                y = i;
                return pItem;
            }
        }
    }

    return NULL;

    __END_CATCH
}

Item* findItemIID(Creature* pCreature, ItemID_t id, Item::ItemClass IClass, int& storage, int& x, int& y)

{
    __BEGIN_TRY

    if (pCreature == NULL)
        return NULL;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    Inventory* pInventory = pPC->getInventory();
    Stash* pStash = pPC->getStash();
    Item* pItem = NULL;
    CoordInven_t tx = 0;
    CoordInven_t ty = 0;

    // Search the inventory.
    pItem = pInventory->findItemIID(id, IClass, tx, ty);
    if (pItem != NULL) {
        storage = STORAGE_INVENTORY;
        x = tx;
        y = ty;
        return pItem;
    }

    // Search the gear slots.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        for (int i = 0; i < Slayer::WEAR_MAX; i++) {
            pItem = pSlayer->getWearItem((Slayer::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        for (int i = 0; i < Vampire::VAMPIRE_WEAR_MAX; i++) {
            pItem = pVampire->getWearItem((Vampire::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        for (int i = 0; i < Ousters::OUSTERS_WEAR_MAX; i++) {
            pItem = pOusters->getWearItem((Ousters::WearPart)i);
            if (pItem != NULL && pItem->getItemID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_GEAR;
                x = i;
                return pItem;
            }
        }
    }

    // Search the mouse slot.
    pItem = pPC->getExtraInventorySlotItem();
    if (pItem != NULL && pItem->getItemID() == id && pItem->getItemClass() == IClass) {
        storage = STORAGE_EXTRASLOT;
        return pItem;
    }

    // Search the stash.
    for (int r = 0; r < STASH_RACK_MAX; r++) {
        for (int i = 0; i < STASH_INDEX_MAX; i++) {
            pItem = pStash->get(r, i);
            if (pItem != NULL && pItem->getItemID() == id && pItem->getItemClass() == IClass) {
                storage = STORAGE_STASH;
                x = r;
                y = i;
                return pItem;
            }
        }
    }

    return NULL;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Compute experience.
// Computes the experience gained when a vampire blood-drains or kills a
// creature.
////////////////////////////////////////////////////////////////////////////////
int computeCreatureExp(Creature* pCreature, int percent, Ousters* pOusters)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    Assert(percent >= 0 && percent <= 100);

    int exp = 0;

    if (pCreature->isSlayer()) {
        // No experience is given when the creature dies again while KILL_AFTERMATH
        // is set, or is blood-drained again while AFTERMATH is set.
        if ((pCreature->isFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH) == true && percent != BLOODDRAIN_EXP) ||
            (pCreature->isFlag(Effect::EFFECT_CLASS_AFTERMATH) == true && percent == BLOODDRAIN_EXP)) {
            exp = 0;
        } else {
            // change exp to 1,chengh modified 2005 11 06
            exp = 1;
        }

        // When a slayer is blood-drained, swap the BLOODDRAIN and KILL exp values.
        if (de::gameContext().combatInfo().isCombat()) {
            if (percent == BLOODDRAIN_EXP)
                percent = KILL_EXP;
            else if (percent == KILL_EXP)
                percent = BLOODDRAIN_EXP;
        }
    } else if (pCreature->isVampire()) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH))
            exp = 0;
        else if (pOusters != NULL) {
            // change exp to 1,chengh modified 2005 11 06
            exp = 1;
        }
    } else if (pCreature->isOusters()) {
        // No experience is given when the creature dies again while KILL_AFTERMATH
        // is set, or is blood-drained again while AFTERMATH is set.
        if ((pCreature->isFlag(Effect::EFFECT_CLASS_KILL_AFTERMATH) == true && percent == KILL_EXP) ||
            (pCreature->isFlag(Effect::EFFECT_CLASS_AFTERMATH) == true && percent == BLOODDRAIN_EXP) ||
            pOusters != NULL) {
            exp = 0;
        } else {
            // change exp to 1,chengh modified 2005 11 06
            exp = 1;
        }
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);
        // Monster type 753 gives no experience.
        if (pMonster->getMonsterType() == 753) {
            return 0;
        }
        if (pMonster->getMonsterType() == GROUND_ELEMENTAL_TYPE) {
            return 0;
        }

        // Ousters are handled separately.
        if (pOusters != NULL) {
            return getPercentValue(getPercentValue((int)(pMonster->getOustersExp(pOusters)),
                                                   de::gameContext().variables().getVariable(MONSTER_EXP_RATIO)),
                                   percent);
        }

        exp += pMonster->getSTR();
        exp += pMonster->getDEX();
        exp += pMonster->getINT();
        exp = (int)(exp * (0.75 + (double)(pMonster->getLevel() / 200.0)));

        // The May 15 event monsters cannot be blood-drained, so death gives the full reward.
        MonsterType_t MonsterType = pMonster->getMonsterType();
        if (MonsterType == 358 || MonsterType == 359)
            exp = 1144;
        else if (MonsterType == 360 || MonsterType == 361)
            exp = 1076;

        // cout << pMonster->getName() << " exp = " << exp << "  percent=" << percent << endl;
        //  Compute the experience change caused by the Enhance field.
    } else
        Assert(false);

    // Code for the stat compensation.
    // exp = (int)((float)exp * 1.5);
    exp = getPercentValue(exp, percent);
    exp = getPercentValue(exp, de::gameContext().variables().getVariable(MONSTER_EXP_RATIO));

    return exp;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Same as computeCreatureExp, but because of AFTERMATH this computes only the HP
// gained when blood draining. Duplicated code.
////////////////////////////////////////////////////////////////////////////////
int computeBloodDrainHealPoint(Creature* pCreature, int percent)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);
    Assert(percent >= 0 && percent <= 100);

    int exp = 0;

    if (pCreature->isSlayer()) {
        // change exp to 1,chengh modified 2005 11 06
        exp = 1;

        // When a slayer is blood-drained, swap the BLOODDRAIN and KILL exp values.
        if (de::gameContext().combatInfo().isCombat()) {
            if (percent == BLOODDRAIN_EXP)
                percent = KILL_EXP;
            else if (percent == KILL_EXP)
                percent = BLOODDRAIN_EXP;
        }
    } else if (pCreature->isVampire()) {
    } else if (pCreature->isOusters()) {
        // change exp to 1,chengh modified 2005 11 06
        exp = 1;
    } else if (pCreature->isMonster()) {
        Monster* pMonster = dynamic_cast<Monster*>(pCreature);

        // Monster type 753 gives no experience.
        if (pMonster->getMonsterType() == 753) {
            return 0;
        }

        exp += pMonster->getSTR();
        exp += pMonster->getDEX();
        exp += pMonster->getINT();
        exp = (int)(exp * (0.75 + (double)(pMonster->getLevel() / 200.0)));

        // The May 15 event monsters cannot be blood-drained, so death gives the full reward.
        MonsterType_t MonsterType = pMonster->getMonsterType();
        if (MonsterType == 358 || MonsterType == 359)
            exp = 1144;
        else if (MonsterType == 360 || MonsterType == 361)
            exp = 1076;

        // cout << pMonster->getName() << " exp = " << exp << "  percent=" << percent << endl;
        //  Compute the experience change caused by the Enhance field.
    } else
        Assert(false);

    // Code for the stat compensation.
    // exp = (int)((float)exp * 1.5);
    exp = getPercentValue(exp, percent);

    return exp;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Check whether the creature is an event monster.
////////////////////////////////////////////////////////////////////////////////
bool isEventMonster(Creature* pCreature)

{
    __BEGIN_TRY

    Assert(pCreature != NULL);

    if (!pCreature->isMonster())
        return false;

    Monster* pMonster = dynamic_cast<Monster*>(pCreature);

    switch (pMonster->getMonsterType()) {
    case 358:
    case 359:
    case 360:
    case 361:
        return true;
    default:
        break;
    }

    return false;

    __END_CATCH
}

////////////////////////////////////////////////////////////////////////////////
// Check whether a creature is currently able to move.
////////////////////////////////////////////////////////////////////////////////
bool isAbleToMove(Creature* pCreature) {
    Assert(pCreature != NULL);

    // If under Hide...
    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE)) {
        if (pCreature->isVampire())
            return false;

        // Monsters can move while hidden.
        // if (pCreature->isMonster()) return false;

        if (pCreature->isSlayer())
            return false;
    }

    // If currently dead...
    if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA) ||
        pCreature->isDead()
        // If currently paralyzed...
        || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE)
        //		|| pCreature->isFlag(Effect::EFFECT_CLASS_SANCTUARY)
        || pCreature->isFlag(Effect::EFFECT_CLASS_CASKET)
        // If currently under Cause Critical Wounds...
        || pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP)
        // 2003.1.2 by Sequoia
        || pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON)
        // 2003.3.31 by Sequoia
        || pCreature->isFlag(Effect::EFFECT_CLASS_POISON_MESH) || pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) || pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER))
        return false;

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Check whether a creature is currently able to use a skill.
////////////////////////////////////////////////////////////////////////////////
bool isAbleToUseSelfSkill(Creature* pCreature, SkillType_t SkillType) {
    Assert(pCreature != NULL);
    if (pCreature->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;

    // Skills cannot be used while hidden.
    // While dead, no skill other than Eternity can be used.
    // Skills cannot be used while invisible.
    // Skills cannot be used while paralyzed.
    // Skills cannot be used while in bat form.
    // Skills cannot be used while under Cause Critical Wounds.
    if ((pCreature->isDead() || pCreature->isFlag(Effect::EFFECT_CLASS_COMA)) && SkillType != SKILL_ETERNITY)
        return false;

    // Skills cannot be used while under Dragon Eye.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE))
        return false;

    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) || pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) || pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM) ||
        (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) && SkillType != SKILL_UN_TRANSFORM) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP) || pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) || pCreature->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) || pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER)) {
        return false;
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) && SkillType != SKILL_OPEN_CASKET) {
        return false;
    }

    // In wolf form every skill request is rejected.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        return false;
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        if (SkillType != SKILL_BITE_OF_DEATH && SkillType != SKILL_UN_TRANSFORM && SkillType != SKILL_RAPID_GLIDING) {
            return false;
        }
    }

    // While riding a sylph only Untransform can be used.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH)) {
        if (SkillType != SKILL_UN_TRANSFORM)
            return false;
    }

    // Skills cannot be used while riding a motorcycle.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->hasRideMotorcycle()) {
            return false;
        }
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET)) {
        if (SkillType != SKILL_UN_TRANSFORM && SkillType != SKILL_TURRET_FIRE)
            return false;
    }

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

bool isAbleToUseObjectSkill(Creature* pCreature, SkillType_t SkillType) {
    Assert(pCreature != NULL);
    if (pCreature->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;

    // Skills cannot be used while under Dragon Eye.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE))
        return false;

    // Skills cannot be used while hidden.
    // Skills cannot be used while dead.
    // Skills cannot be used while invisible.
    // Skills cannot be used while paralyzed.
    // Skills cannot be used while in bat form.
    // Skills cannot be used while under Cause Critical Wounds.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) || pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isDead() ||
        pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP) || pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) || pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) || pCreature->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) || pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER))
        return false;

    // In wolf form every skill request is rejected.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        return false;
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        if (SkillType != SKILL_BITE_OF_DEATH && SkillType != SKILL_UN_TRANSFORM && SkillType != SKILL_ATTACK_MELEE &&
            SkillType != SKILL_RAPID_GLIDING) {
            return false;
        }
    }

    // Skills cannot be used while riding a motorcycle.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->hasRideMotorcycle())
            return false;
    }

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

bool isAbleToUseTileSkill(Creature* pCreature, SkillType_t SkillType) {
    Assert(pCreature != NULL);
    if (pCreature->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;

    // Skills cannot be used while under Dragon Eye.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE))
        return false;

    // Skills cannot be used while hidden.
    // Skills cannot be used while dead.
    // Skills cannot be used while invisible.
    // Skills cannot be used while paralyzed.
    // Skills cannot be used while in bat form.
    // Skills cannot be used while under Cause Critical Wounds.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) || pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isDead() ||
        pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP) || pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) || pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) || pCreature->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) || pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER))
        return false;

    // In wolf form every skill request is rejected.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        return false;
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        if (SkillType != SKILL_BITE_OF_DEATH && SkillType != SKILL_UN_TRANSFORM && SkillType != SKILL_ATTACK_MELEE &&
            SkillType != SKILL_RAPID_GLIDING) {
            return false;
        }
    }

    // Skills cannot be used while riding a motorcycle.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->hasRideMotorcycle())
            return false;
    }

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

bool isAbleToUseInventorySkill(Creature* pCreature, BYTE X, BYTE Y, BYTE TX, BYTE TY, SkillType_t SkillType) {
    Assert(pCreature != NULL);
    if (pCreature->isFlag(Effect::EFFECT_CLASS_PLEASURE_EXPLOSION))
        return false;

    // Skills cannot be used while under Dragon Eye.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE))
        return false;

    // Skills cannot be used when the inventory coordinates are out of range.
    // Skills cannot be used while hidden.
    // Skills cannot be used while dead.
    // Skills cannot be used while invisible.
    // Skills cannot be used while paralyzed.
    // Skills cannot be used while in bat form.
    // Skills cannot be used while under Cause Critical Wounds.
    // if (X >= 10 || Y >= 6 || TX >= 10 || TY >= 6) return false;
    if (X >= 10 || Y >= 6)
        return false;
    if (pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) || pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_COMA) || pCreature->isDead() ||
        pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_CAUSE_CRITICAL_WOUNDS) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SOUL_CHAIN) || pCreature->isFlag(Effect::EFFECT_CLASS_LOVE_CHAIN) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_GUN_SHOT_GUIDANCE_AIM) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SLEEP) || pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) || pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) || pCreature->isFlag(Effect::EFFECT_CLASS_REFINIUM_TICKET) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) || pCreature->isFlag(Effect::EFFECT_CLASS_EXPLOSION_WATER))
        return false;

    // In wolf form every skill request is rejected.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF)) {
        return false;
    }

    if (pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WERWOLF)) {
        if (SkillType != SKILL_BITE_OF_DEATH && SkillType != SKILL_UN_TRANSFORM && SkillType != SKILL_ATTACK_MELEE &&
            SkillType != SKILL_RAPID_GLIDING) {
            return false;
        }
    }

    // Skills cannot be used while riding a motorcycle.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->hasRideMotorcycle())
            return false;
    }

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Check whether a creature is currently able to pick up an item.
////////////////////////////////////////////////////////////////////////////////
bool isAbleToPickupItem(Creature* pCreature, Item* pItem) {
    Assert(pCreature != NULL);

    // Items cannot be picked up while dead.
    // Items cannot be picked up while in bat form.
    if (pCreature->isFlag(Effect::EFFECT_CLASS_COMA)

        // If currently paralyzed...
        || pCreature->isFlag(Effect::EFFECT_CLASS_PARALYZE) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ETERNITY_PAUSE)
        //		|| pCreature->isFlag(Effect::EFFECT_CLASS_SANCTUARY)

        || pCreature->isFlag(Effect::EFFECT_CLASS_CASKET) || pCreature->isDead() ||
        pCreature->isFlag(Effect::EFFECT_CLASS_ARMAGEDDON) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_SUMMON_SYLPH) || pCreature->isFlag(Effect::EFFECT_CLASS_TENDRIL) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_BLOCK_HEAD) || pCreature->isFlag(Effect::EFFECT_CLASS_TRAPPED) ||
        pCreature->isFlag(Effect::EFFECT_CLASS_INSTALL_TURRET))
        return false;

    // Nothing can be picked up inside a sanctuary.
    Zone* pZone = pCreature->getZone();
    if (pZone != NULL) {
        Tile& rTile = pZone->getTile(pCreature->getX(), pCreature->getY());
        if (rTile.getEffect(Effect::EFFECT_CLASS_SANCTUARY) != NULL) {
            return false;
        }
    }

    // Items cannot be picked up while riding a motorcycle.
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        if (pSlayer->hasRideMotorcycle())
            return false;
    }

    // Only one QuestItem may be held at a time.
    Item::ItemClass itemClass = pItem->getItemClass();
    ItemType_t itemtype = pItem->getItemType();

    if (itemClass == Item::ITEM_CLASS_QUEST_ITEM && pCreature->isPC() && pItem->getItemType() < 4) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Inventory* pInventory = pPC->getInventory();

        // Either held on the mouse cursor,
        Item* pMouseItem = pPC->getExtraInventorySlotItem();

        if (pMouseItem != NULL && pMouseItem->getItemClass() == Item::ITEM_CLASS_QUEST_ITEM &&
            pMouseItem->getItemType() < 4)
            return false;

        // or present in the inventory.
        Item* pInvenItem = pInventory->findItem(Item::ITEM_CLASS_QUEST_ITEM);

        if (pInvenItem != NULL && pInvenItem->getItemClass() == Item::ITEM_CLASS_QUEST_ITEM &&
            pInvenItem->getItemType() < 4)
            return false;
    }

    if (itemClass == Item::ITEM_CLASS_EVENT_ITEM && pCreature->isPC() && pItem->getItemType() == 30) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        Inventory* pInventory = pPC->getInventory();

        Item* pMouseItem = pPC->getExtraInventorySlotItem();
        if (pMouseItem != NULL && pMouseItem->getItemClass() == Item::ITEM_CLASS_EVENT_ITEM &&
            pMouseItem->getItemType() == 30)
            return false;

        Item* pInvenItem = pInventory->findItem(Item::ITEM_CLASS_EVENT_ITEM, (ItemType_t)30);
        if (pInvenItem != NULL)
            return false;
    }

    // Quest items (time-limited items) cannot be picked up.
    if (pItem->isTimeLimitItem())
        return false;

    // Relic items: the relic, the blood bible, the castle symbol and the like.
    if (isRelicItem(itemClass)) {
        // A relic cannot be picked up while the relic-lock effect is on it, or
        // while the creature is using certain skills.
        if (pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK) || pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) || pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_HAS_BLOOD_BIBLE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_DRAGON_EYE)) {
            return false;
        }

        switch (itemClass) {
        // Only a relic that does not exist yet can be picked up.
        case Item::ITEM_CLASS_RELIC: {
            const RelicInfo* pRelicInfo =
                dynamic_cast<RelicInfo*>(de::gameContext().itemInfos().getItemInfo(Item::ITEM_CLASS_RELIC, itemtype));

            if ((pRelicInfo->relicType == RELIC_TYPE_SLAYER &&
                 !pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SLAYER_RELIC)) ||
                (pRelicInfo->relicType == RELIC_TYPE_VAMPIRE &&
                 !pCreature->isFlag(Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC))) {
                return true;
            }
        } break;

        case Item::ITEM_CLASS_BLOOD_BIBLE: {
            if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC) ||
                pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SLAYER_RELIC)) {
                return false;
            }

            BloodBible* pBloodBible = dynamic_cast<BloodBible*>(pItem);
            Assert(pBloodBible != NULL);

            if (!de::gameContext().shrines().canPickupBloodBible(pCreature->getRace(), pBloodBible)) {
                return false;
            }

            // Cannot be picked up if the picker is in a safe zone.
            ZoneLevel_t zoneLevel = pCreature->getZone()->getZoneLevel(pCreature->getX(), pCreature->getY());
            if (zoneLevel & SAFE_ZONE) {
                return false;
            }
        } break;

        case Item::ITEM_CLASS_CASTLE_SYMBOL: {
            if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_VAMPIRE_RELIC) ||
                pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SLAYER_RELIC)) {
                return false;
            }

            ZoneID_t castleZoneID = 0;
            if (de::gameContext().castleInfos().getCastleZoneID(pCreature->getZone()->getZoneID(), castleZoneID)) {
                CastleInfo* pCastleInfo = de::gameContext().castleInfos().getCastleInfo(castleZoneID);
                if (pCreature->getRace() != pCastleInfo->getRace())
                    return false;
            } else {
                if (pCreature->getZoneID() == 1500)
                    return true;
                return false;
            }
        } break;

        case Item::ITEM_CLASS_WAR_ITEM: {
            // Only level 150 and above can carry it.
            Level_t level = pCreature->getLevel();
            if (level < 150)
                return false;
        } break;

        default:
            return false;
        }
    }

    if (pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))
            return false;
        LevelWarManager* pLevelWarManager = NULL;
        if (pZone != NULL)
            pLevelWarManager = pZone->getLevelWarManager();

        if (pLevelWarManager == NULL || !pLevelWarManager->hasWar())
            return false;

        if (pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK) || pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) || pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL))
            return false;
    }

    // A player already holding a flag cannot pick up a flag.
    if (pItem->isFlagItem()) {
        if (!de::gameContext().flags().hasFlagWar())
            return false;
        if (pCreature->isFlag(Effect::EFFECT_CLASS_HAS_FLAG))
            return false;
        if (pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK) || pCreature->isFlag(Effect::EFFECT_CLASS_HIDE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) || pCreature->isFlag(Effect::EFFECT_CLASS_FADE_OUT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_WOLF) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_TRANSFORM_TO_BAT) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
            pCreature->isFlag(Effect::EFFECT_CLASS_HAS_CASTLE_SYMBOL))
            return false;
    }

    ///*
    if (pCreature->isSlayer()) {
        if (pItem->isFlag(Effect::EFFECT_CLASS_VAMPIRE_ONLY))
            return false;
        if (pItem->isFlag(Effect::EFFECT_CLASS_OUSTERS_ONLY))
            return false;

        // A Slayer cannot pick up vampire items.
        // Ousters items cannot be picked up either.
        switch (itemClass) {
        case Item::ITEM_CLASS_VAMPIRE_RING:
        case Item::ITEM_CLASS_VAMPIRE_BRACELET:
        case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
        case Item::ITEM_CLASS_VAMPIRE_COAT:
        case Item::ITEM_CLASS_VAMPIRE_ETC:
        case Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM:
        case Item::ITEM_CLASS_VAMPIRE_EARRING:
        case Item::ITEM_CLASS_VAMPIRE_WEAPON:
        case Item::ITEM_CLASS_VAMPIRE_AMULET:
        case Item::ITEM_CLASS_SERUM:
        case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
        case Item::ITEM_CLASS_OUSTERS_BOOTS:
        case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
        case Item::ITEM_CLASS_OUSTERS_CIRCLET:
        case Item::ITEM_CLASS_OUSTERS_COAT:
        case Item::ITEM_CLASS_OUSTERS_PENDENT:
        case Item::ITEM_CLASS_OUSTERS_RING:
        case Item::ITEM_CLASS_OUSTERS_STONE:
        case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        case Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM:
        case Item::ITEM_CLASS_CODE_SHEET:
        case Item::ITEM_CLASS_DERMIS:
        case Item::ITEM_CLASS_PERSONA:
        case Item::ITEM_CLASS_FASCIA:
        case Item::ITEM_CLASS_MITTEN:
            // case Item::ITEM_CLASS_MONEY :
            //  Vampire money cannot be picked up.
            //  edit by sonic 2006.10.31
            // if (pItem->getItemType()==1) return false;
            return false;


            break;

        default:
            return true;
        }

        return true;
    } else if (pCreature->isVampire()) {
        if (pItem->isFlag(Effect::EFFECT_CLASS_SLAYER_ONLY))
            return false;
        if (pItem->isFlag(Effect::EFFECT_CLASS_OUSTERS_ONLY))
            return false;

        // A Vampire can pick up only vampire items and
        // event items.
        switch (pItem->getItemClass()) {
        case Item::ITEM_CLASS_VAMPIRE_RING:
        case Item::ITEM_CLASS_VAMPIRE_BRACELET:
        case Item::ITEM_CLASS_VAMPIRE_NECKLACE:
        case Item::ITEM_CLASS_VAMPIRE_COAT:
        case Item::ITEM_CLASS_VAMPIRE_ETC:
        case Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM:
        case Item::ITEM_CLASS_VAMPIRE_WEAPON:
        case Item::ITEM_CLASS_VAMPIRE_AMULET:
        case Item::ITEM_CLASS_VAMPIRE_EARRING:
        case Item::ITEM_CLASS_SERUM:

        case Item::ITEM_CLASS_SKULL:
        case Item::ITEM_CLASS_EVENT_GIFT_BOX:
        case Item::ITEM_CLASS_EVENT_STAR:
        case Item::ITEM_CLASS_RELIC:
        case Item::ITEM_CLASS_QUEST_ITEM:
        case Item::ITEM_CLASS_EVENT_TREE:
        case Item::ITEM_CLASS_EVENT_ETC:
        case Item::ITEM_CLASS_BLOOD_BIBLE:
        case Item::ITEM_CLASS_CASTLE_SYMBOL:
        case Item::ITEM_CLASS_EVENT_ITEM:
        case Item::ITEM_CLASS_RESURRECT_ITEM:
        case Item::ITEM_CLASS_MIXING_ITEM:
        case Item::ITEM_CLASS_EFFECT_ITEM:
        case Item::ITEM_CLASS_DYE_POTION:
        case Item::ITEM_CLASS_MOON_CARD:
        case Item::ITEM_CLASS_SWEEPER:
        case Item::ITEM_CLASS_PET_ITEM:
        case Item::ITEM_CLASS_PET_FOOD:
        case Item::ITEM_CLASS_PET_ENCHANT_ITEM:
        case Item::ITEM_CLASS_LUCKY_BAG:
        case Item::ITEM_CLASS_SMS_ITEM:
        case Item::ITEM_CLASS_CORE_ZAP:
        case Item::ITEM_CLASS_TRAP_ITEM:
        case Item::ITEM_CLASS_WAR_ITEM:
        case Item::ITEM_CLASS_DERMIS:
        case Item::ITEM_CLASS_PERSONA:
        case Item::ITEM_CLASS_MONEY:
            // Vampire money can be picked up.
            // edit by sonic 2006.10.31
            // if (pItem->getItemType()==1) return true;
            return true;


            break;

        default:
            return false;
        }


        return false;
    } else if (pCreature->isOusters()) {
        if (pItem->isFlag(Effect::EFFECT_CLASS_SLAYER_ONLY))
            return false;
        if (pItem->isFlag(Effect::EFFECT_CLASS_VAMPIRE_ONLY))
            return false;

        // An Ousters can pick up only ousters items and
        // event items.
        switch (pItem->getItemClass()) {
        case Item::ITEM_CLASS_OUSTERS_ARMSBAND:
        case Item::ITEM_CLASS_OUSTERS_BOOTS:
        case Item::ITEM_CLASS_OUSTERS_CHAKRAM:
        case Item::ITEM_CLASS_OUSTERS_CIRCLET:
        case Item::ITEM_CLASS_OUSTERS_COAT:
        case Item::ITEM_CLASS_OUSTERS_PENDENT:
        case Item::ITEM_CLASS_OUSTERS_RING:
        case Item::ITEM_CLASS_OUSTERS_STONE:
        case Item::ITEM_CLASS_OUSTERS_WRISTLET:
        case Item::ITEM_CLASS_SKULL:
        case Item::ITEM_CLASS_EVENT_GIFT_BOX:
        case Item::ITEM_CLASS_EVENT_STAR:
        case Item::ITEM_CLASS_RELIC:
        case Item::ITEM_CLASS_QUEST_ITEM:
        case Item::ITEM_CLASS_EVENT_TREE:
        case Item::ITEM_CLASS_EVENT_ETC:
        case Item::ITEM_CLASS_BLOOD_BIBLE:
        case Item::ITEM_CLASS_CASTLE_SYMBOL:
        case Item::ITEM_CLASS_EVENT_ITEM:
        case Item::ITEM_CLASS_RESURRECT_ITEM:
        case Item::ITEM_CLASS_MIXING_ITEM:
        case Item::ITEM_CLASS_LARVA:
        case Item::ITEM_CLASS_PUPA:
        case Item::ITEM_CLASS_COMPOS_MEI:
        case Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM:
        case Item::ITEM_CLASS_EFFECT_ITEM:
        case Item::ITEM_CLASS_DYE_POTION:
        case Item::ITEM_CLASS_MOON_CARD:
        case Item::ITEM_CLASS_SWEEPER:
        case Item::ITEM_CLASS_PET_ITEM:
        case Item::ITEM_CLASS_PET_FOOD:
        case Item::ITEM_CLASS_PET_ENCHANT_ITEM:
        case Item::ITEM_CLASS_LUCKY_BAG:
        case Item::ITEM_CLASS_SMS_ITEM:
        case Item::ITEM_CLASS_CORE_ZAP:
        case Item::ITEM_CLASS_TRAP_ITEM:
        case Item::ITEM_CLASS_WAR_ITEM:
        case Item::ITEM_CLASS_FASCIA:
        case Item::ITEM_CLASS_MITTEN:
        case Item::ITEM_CLASS_MONEY:
            // Ousters money can be picked up.
            // edit by sonic 2006.10.31
            // if (pItem->getItemType()==1) return true;
            return true;


            break;

        default:
            return false;
        }

        return false;
    }
    //*/

    if (pCreature->isPC()) {
        if (dynamic_cast<PlayerCreature*>(pCreature)->getStore()->isOpen())
            return false;
    }

    return true;
}

bool canDropToZone(Creature* pCreature, Item* pItem) {
    // Quest items (time-limited items) cannot be dropped.
    if (pItem->isTimeLimitItem())
        return false;

    if (!pCreature->isPC())
        return false;
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_MONEY: {
        // edit by sonic
        // return false;
    }
    case Item::ITEM_CLASS_COUPLE_RING:
    case Item::ITEM_CLASS_VAMPIRE_COUPLE_RING: {
        if (PartnerWaitInfo::getItemClass(pPC) == pItem->getItemClass() &&
            PartnerWaitInfo::getItemType(pPC) == pItem->getItemType()) {
            return false;
        }
    } break;

    case Item::ITEM_CLASS_MOON_CARD:
        // Half-moon cards cannot be dropped on the ground.
        {
            if (pItem->getItemType() == 0)
                return false;
        }
        break;
    case Item::ITEM_CLASS_LUCKY_BAG: {
        if (pItem->getItemType() == 3)
            return false;
    } break;
    case Item::ITEM_CLASS_EVENT_ITEM: {
        if (pItem->getItemType() == 28 || pItem->getItemType() == 30)
            return false;
    } break;
    case Item::ITEM_CLASS_EFFECT_ITEM: {
        if (pItem->getItemType() >= 4 && pItem->getItemType() <= 6)
            return false;
    } break;
    case Item::ITEM_CLASS_EVENT_STAR: {
        if (pItem->getItemType() >= 17 && pItem->getItemType() <= 21)
            return false;
    } break;
    case Item::ITEM_CLASS_MIXING_ITEM: {
        if (pItem->getItemType() == 18)
            return false;
    } break;

    default:
        break;
    }

    // Exchange System: Point-only items cannot be dropped
    if (isPointOnlyTradeItem(pItem))
        return false;

    return true;
}

// Called for a grand master when the grand master effect cannot be sent to the client.
// 2002. 1. 13. Sequoia
// Not used.

bool getRaceFromDB(const string& Name, Race_t& race)

{
    __BEGIN_TRY

    {
        string Race;

        if (defaultCharacterRepository().loadSlayerRaceText(Name, Race)) {
            if (Race == "SLAYER") {
                race = RACE_SLAYER;
            } else if (Race == "VAMPIRE") {
                race = RACE_VAMPIRE;
            } else
                race = RACE_OUSTERS;
        } else {
            return false;
        }
    }

    return true;

    __END_CATCH
}

bool getGuildIDFromDB(const string& Name, Race_t race, GuildID_t& guildID)

{
    __BEGIN_TRY

    {
        CharacterRace table;
        if (race == RACE_SLAYER)
            table = CHARACTER_RACE_SLAYER;
        else if (race == RACE_VAMPIRE)
            table = CHARACTER_RACE_VAMPIRE;
        else
            table = CHARACTER_RACE_OUSTERS;

        int guildIDValue = 0;

        if (defaultCharacterRepository().loadGuildID(Name, table, guildIDValue)) {
            guildID = (GuildID_t)guildIDValue;

            if (guildID == 0 || guildID == 99 || guildID == 66) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;

    __END_CATCH
}

bool canSee(const Creature* pSource, Creature* pTarget, EffectObservingEye* pEffectObservingEye,
            EffectGnomesWhisper* pEffectGnomesWhisper) {
    // A target under Ghost can never be seen.
    if (pTarget->isFlag(Effect::EFFECT_CLASS_GHOST))
        return false;

    // ZoneCoord_t targetX = pTarget->getX(), targetY = pTarget->getY();

    // Fetch the ObservingEye effect.
    if (pEffectObservingEye == NULL && pSource->isFlag(Effect::EFFECT_CLASS_OBSERVING_EYE)) {
        pEffectObservingEye =
            dynamic_cast<EffectObservingEye*>(pSource->findEffect(Effect::EFFECT_CLASS_OBSERVING_EYE));
    }

    // Fetch the GnomesWhisper effect.
    if (pEffectGnomesWhisper == NULL && pSource->isFlag(Effect::EFFECT_CLASS_GNOMES_WHISPER)) {
        pEffectGnomesWhisper =
            dynamic_cast<EffectGnomesWhisper*>(pSource->findEffect(Effect::EFFECT_CLASS_GNOMES_WHISPER));
    }

    // The target has to be within sight to be seen at all.
    // if (pSource->getVisionState(targetX, targetY) >= IN_SIGHT)
    //{
    // Vampires always see each other.
    if (pSource->isVampire() && pTarget->isVampire())
        return true;

    if ((!pTarget->isFlag(Effect::EFFECT_CLASS_HIDE) ||
         pSource->isFlag(Effect::EFFECT_CLASS_DETECT_HIDDEN)
         // Revealer can also see a hidden creature.
         || pSource->isFlag(Effect::EFFECT_CLASS_REVEALER) ||
         (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeHide())) &&
        (!pTarget->isFlag(Effect::EFFECT_CLASS_INVISIBILITY) ||
         pSource->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) || pSource->isVampire() ||
         (pEffectObservingEye != NULL && pEffectObservingEye->canSeeInvisibility(pTarget)) ||
         (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeInvisibility())) &&
        (!pTarget->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE) ||
         pSource->isFlag(Effect::EFFECT_CLASS_DETECT_INVISIBILITY) ||
         (pEffectGnomesWhisper != NULL && pEffectGnomesWhisper->canSeeSniping()))) {
        return true;
    }
    //}

    return false;
}

int changeSexEx(PlayerCreature* pPC) {
    Zone* pZone = pPC->getZone();
    if (pZone == NULL)
        return 3;

    if (pPC->getFlagSet()->isOn(FLAGSET_IS_COUPLE))
        return 2;

    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        if (pSlayer->isWear(Slayer::WEAR_BODY) || pSlayer->isWear(Slayer::WEAR_LEG))
            return 1;

        if (pSlayer->getSex() == MALE)
            pSlayer->setSex(FEMALE);
        else
            pSlayer->setSex(MALE);

    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        if (pVampire->isWear(Vampire::WEAR_BODY))
            return 1;

        if (pVampire->getSex() == MALE)
            pVampire->setSex(FEMALE);
        else
            pVampire->setSex(MALE);

    } else
        return 3;

    // Reaching here means the sex change succeeded; update the database.
    // A successful sex change is never an Ousters, so both the slayer and vampire tables have rows.
    defaultCharacterRepository().saveSex(pPC->getName(), Sex2String[pPC->getSex()]);

    return 0;
}

void initAllStatAndSendChange(PlayerCreature* pPC) {
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        Assert(pSlayer != NULL);

        SLAYER_RECORD prev;
        pSlayer->getSlayerRecord(prev);
        pSlayer->initAllStat();
        pSlayer->sendModifyInfo(prev);
        pSlayer->sendRealWearingInfo();
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        Assert(pVampire != NULL);

        VAMPIRE_RECORD prev;
        pVampire->getVampireRecord(prev);
        pVampire->initAllStat();
        pVampire->sendModifyInfo(prev);
        pVampire->sendRealWearingInfo();
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        Assert(pOusters != NULL);

        OUSTERS_RECORD prev;
        pOusters->getOustersRecord(prev);
        pOusters->initAllStat();
        pOusters->sendModifyInfo(prev);
        pOusters->sendRealWearingInfo();
    }
}

void addSimpleCreatureEffect(Creature* pCreature, Effect::EffectClass eClass, int time /* = -1 */,
                             bool isSend /* = true */) {
    SimpleCreatureEffect* pEffect = new SimpleCreatureEffect(eClass, pCreature);
    Assert(pEffect != NULL);

    if (time != -1)
        pEffect->setDeadline(time);
    pEffect->setBroadcastingEffect(isSend);

    pCreature->addEffect(pEffect);
    pCreature->setFlag(eClass);

    if (isSend) {
        GCAddEffect gcAddEffect;
        gcAddEffect.setObjectID(pCreature->getObjectID());
        gcAddEffect.setEffectID(pEffect->getSendEffectClass());

        if (time == -1)
            gcAddEffect.setDuration(65535);
        else {
            if (pEffect->getSendEffectClass() == Effect::EFFECT_CLASS_BLOOD_DRAIN ||
                pEffect->getSendEffectClass() == Effect::EFFECT_CLASS_CAN_ENTER_GDR_LAIR)
                gcAddEffect.setDuration(time / 10);
            else
                gcAddEffect.setDuration(time);
        }

        pCreature->getZone()->broadcastPacket(pCreature->getX(), pCreature->getY(), &gcAddEffect);
    }
}

void deleteCreatureEffect(Creature* pCreature, Effect::EffectClass eClass) {
    Effect* pEffect = pCreature->findEffect(eClass);
    if (pEffect != NULL)
        pEffect->setDeadline(0);
}

bool dropFlagToZone(PlayerCreature* pPC, Item* pItem)

{
    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    // cout << "Flag found" << endl;

    // First drop the item on the ground.
    // It could overlap a corpse, so drop it where no character stands.
    //	TPOINT pt = pZone->addItem( pItem, pPC->getX(), pPC->getY(), false );
    pZone->addItemDelayed(pItem, pPC->getX(), pPC->getY(), false);

    Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_FLAG);
    if (pEffect != NULL)
        pEffect->setDeadline(0);

    if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
        EffectRelicLock* pLock = new EffectRelicLock(pItem);
        pLock->setDeadline(10 * 10); // 10 seconds
        pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
        pItem->getEffectManager().addEffect(pLock);
    }

    return true;
}

bool dropFlagToZone(Creature* pCreature, bool bSendPacket) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    if (!pPC->isFlag(Effect::EFFECT_CLASS_HAS_FLAG))
        return false;

    bool bDrop = false;

    // Check whether a relic is on the mouse cursor.
    Item* pSlotItem = pPC->getExtraInventorySlotItem();

    if (pSlotItem != NULL && pSlotItem->isFlagItem()) {
        if (dropFlagToZone(pPC, pSlotItem)) {
            pPC->deleteItemFromExtraInventorySlot();

            // Remove it from the player's mouse cursor.
            // When the client receives this packet it also
            // checks the mouse slot.

            if (bSendPacket) {
                GCDeleteInventoryItem gcDeleteInventoryItem;
                gcDeleteInventoryItem.setObjectID(pSlotItem->getObjectID());

                pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
            }

            bDrop = true;
        }
    }

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    // Look for a flag item in the inventory.
    for (CoordInven_t y = 0; y < pInventory->getHeight(); y++) {
        for (CoordInven_t x = 0; x < pInventory->getWidth(); x++) {
            Item* pItem = pInventory->getItem(x, y);
            if (pItem != NULL && pItem->isFlagItem()) {
                // First drop the item on the ground.
                if (dropFlagToZone(pPC, pItem)) {
                    // Remove it from the inventory.
                    pInventory->deleteItem(pItem->getObjectID());

                    // Remove it from the player's inventory.
                    if (bSendPacket) {
                        GCDeleteInventoryItem gcDeleteInventoryItem;
                        gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

                        pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
                    }

                    bDrop = true;
                }
            }
        }
    }

    return bDrop;
}

void disableFlags(Creature* pCreature, Zone* pZone, SkillType_t SkillType) {
    if (pCreature->isSlayer()) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_SNIPING_MODE)) {
            g_Sniping.checkRevealRatio(pCreature, 20, 10);
        }
    } else if (pCreature->isVampire() && pZone != NULL) {
        if (pCreature->isFlag(Effect::EFFECT_CLASS_INVISIBILITY)) {
            addVisibleCreature(pZone, pCreature, true);
        }

    } else if (pCreature->isOusters()) {
    }
}

bool canEnterBeginnerZone(Creature* pCreature) {
    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
        Assert(pSlayer != NULL);

        return pSlayer->getTotalAttr(ATTR_BASIC) <= 150;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);
        Assert(pVampire != NULL);

        return pVampire->getLevel() <= 30;
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);
        Assert(pOusters != NULL);

        return pOusters->getLevel() <= 30;
    }

    return false;
}

bool dropSweeperToZone(PlayerCreature* pPC, Item* pItem)

{
    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    pZone->addItemDelayed(pItem, pPC->getX(), pPC->getY(), false);

    Effect* pEffect = pPC->findEffect(Effect::EFFECT_CLASS_HAS_SWEEPER);
    if (pEffect != NULL)
        pEffect->setDeadline(0);

    if (!pItem->isFlag(Effect::EFFECT_CLASS_RELIC_LOCK)) {
        EffectRelicLock* pLock = new EffectRelicLock(pItem);
        pLock->setDeadline(10 * 10); // 10 seconds
        pItem->setFlag(Effect::EFFECT_CLASS_RELIC_LOCK);
        pItem->getEffectManager().addEffect(pLock);
    }

    return true;
}

bool dropSweeperToZone(Creature* pCreature, bool bSendPacket) {
    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
    if (!pPC->isFlag(Effect::EFFECT_CLASS_HAS_SWEEPER))
        return false;

    bool bDrop = false;

    // Check whether a relic is on the mouse cursor.
    Item* pSlotItem = pPC->getExtraInventorySlotItem();

    if (pSlotItem != NULL && pSlotItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
        if (dropSweeperToZone(pPC, pSlotItem)) {
            pPC->deleteItemFromExtraInventorySlot();

            // Remove it from the player's mouse cursor.
            // When the client receives this packet it also
            // checks the mouse slot.

            if (bSendPacket) {
                GCDeleteInventoryItem gcDeleteInventoryItem;
                gcDeleteInventoryItem.setObjectID(pSlotItem->getObjectID());

                pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
            }

            bDrop = true;
        }
    }

    Zone* pZone = pPC->getZone();
    Assert(pZone != NULL);

    Inventory* pInventory = pPC->getInventory();
    Assert(pInventory != NULL);

    // Look for a sweeper item in the inventory.
    for (CoordInven_t y = 0; y < pInventory->getHeight(); y++) {
        for (CoordInven_t x = 0; x < pInventory->getWidth(); x++) {
            Item* pItem = pInventory->getItem(x, y);
            if (pItem != NULL && pItem->getItemClass() == Item::ITEM_CLASS_SWEEPER) {
                // First drop the item on the ground.
                if (dropSweeperToZone(pPC, pItem)) {
                    // Remove it from the inventory.
                    pInventory->deleteItem(pItem->getObjectID());

                    // Remove it from the player's inventory.
                    if (bSendPacket) {
                        GCDeleteInventoryItem gcDeleteInventoryItem;
                        gcDeleteInventoryItem.setObjectID(pItem->getObjectID());

                        pPC->getPlayer()->sendPacket(&gcDeleteInventoryItem);
                    }

                    bDrop = true;
                }
            }
        }
    }

    return bDrop;
}

Level_t getPCLevel(PlayerCreature* pPC) {
    if (pPC->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pPC);
        return pSlayer->getHighestSkillDomainLevel();
    } else if (pPC->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pPC);
        return pVampire->getLevel();
    } else if (pPC->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pPC);
        return pOusters->getLevel();
    }

    return 0;
}

void sendPetInfo(GamePlayer* pGamePlayer, bool bBroadcast, bool bSummon) {
    if (pGamePlayer == NULL)
        return;

    PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pGamePlayer->getCreature());
    if (pPC == NULL)
        return;

    PetInfo* pPetInfo = pPC->getPetInfo();

    GCPetInfo gcPetInfo;
    gcPetInfo.setPetInfo(pPetInfo);
    gcPetInfo.setSummonInfo((bSummon) ? 1 : 0);
    gcPetInfo.setObjectID(pPC->getObjectID());
    pGamePlayer->sendPacket(&gcPetInfo);

    if (bBroadcast) {
        pPC->getZone()->broadcastPacket(pPC->getX(), pPC->getY(), &gcPetInfo, pPC);
    }
}

void giveGoldMedal(PlayerCreature* pPC) {
    __BEGIN_TRY

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
    Assert(pGamePlayer != NULL);

    {
        defaultPlayRecordRepository().insertGoldMedal(pGamePlayer->getID());
        addSimpleCreatureEffect(pPC, Effect::EFFECT_CLASS_GOLD_MEDAL, 10, true);

        GCSystemMessage gcSM;
        gcSM.setMessage("You received an Athens gold medal.");
        pGamePlayer->sendPacket(&gcSM);
        // An older, disabled flow kept a per-account GoldMedalCount and
        // sent it back as GCNoticeEvent NOTICE_EVENT_GOLD_MEDALS; the
        // lotto counter below (addLotto) has that shape.
    }

    __END_CATCH
}

void giveLotto(PlayerCreature* pPC, BYTE type, uint num) {
    __BEGIN_TRY

    GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
    Assert(pGamePlayer != NULL);

    {
        int count = 0;

        if (defaultPlayRecordRepository().addLotto(pGamePlayer->getID(), type, num, count)) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                     "You now hold %d event lottery tickets. See the official homepage for details.", count);
            GCSystemMessage gcSM;
            gcSM.setMessage(buffer);
            pGamePlayer->sendPacket(&gcSM);
        }
    }

    __END_CATCH
}

void addOlympicStat(PlayerCreature* pPC, BYTE type, uint num) {
    __BEGIN_TRY

    // Records nothing: the olympic event's OlympicStat counters are not kept.
    __END_CATCH
}

void deletePC(PlayerCreature* pPC) {
    __BEGIN_TRY

    // The 109 statements that retire a character's rows -- the three race
    // tables' Active='INACTIVE' updates, the skill saves and rank bonus, the
    // 81 item-object tables, GQuestSave, CoupleInfo, the persisted effects,
    // FlagSet, TimeLimitItems and EventQuestAdvance -- run in that order on one
    // Statement in CharacterPurgeRepository::purgeCharacter.
    defaultCharacterPurgeRepository().purgeCharacter(pPC->getName());

    __END_CATCH
}

bool isAffectExp2X() {
    if (de::gameContext().variables().getVariable(TIME_PERIOD_EXP_2X) != 0) {
        TimeChecker& timeChecker = de::gameContext().timeChecker();
        if (timeChecker.isInPeriod(TIME_PERIOD_AFTER_SCHOOL) || timeChecker.isInPeriod(TIME_PERIOD_AFTER_WORK) ||
            timeChecker.isInPeriod(TIME_PERIOD_MIDNIGHT)) {
            return true;
        }
    }

    return false;
}
