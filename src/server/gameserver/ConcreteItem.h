#ifndef __CONCRETE_ITEM_H__
#define __CONCRETE_ITEM_H__

#include "DB.h"
#include "GameContext.h"
#include "Item.h"
#include "ItemInfo.h"
#include "ItemInfoManager.h"
#include "ItemPolicies.h"

// An item's own maximum durability, before its options: the item table's
// durability moved by the grade and floored at 1000 when the class tracks
// durability, the table value otherwise. Defined in ItemUtil.cpp over
// decore::maxDurabilityBase, so this header stays free of de-core includes.
Durability_t computeBaseMaxDurability(Durability_t infoDurability, bool hasDurability, int gradeDurabilityOffset);

// How far an item's grade moves each of its attributes (decore::GradeOffsets).
struct ItemGradeOffsets {
    int durability;
    int damage;
    int critical;
    int defense;
    int protection;
    int luck;
};

// An item class's grade and durability rules, read from de-core's per-class
// table (domain/ItemGrade.h): whether the class keeps a grade
// (decore::gradePolicyOf is not None), whether it keeps a durability
// (decore::hasDurability), and the offsets a grade gives under the class's
// grade policy (decore::gradeOffsets). Defined in ItemUtil.cpp, so this
// header stays free of de-core includes.
bool itemClassHasGrade(Item::ItemClass itemClass);
bool itemClassHasDurability(Item::ItemClass itemClass);
ItemGradeOffsets itemClassGradeOffsets(Item::ItemClass itemClass, Grade_t grade);

// An item built from policies. Its grade and durability rules are not
// template arguments: they come from de-core's table for IClass, the same
// table the client builds, so the two cannot disagree. A class without a
// grade reads -1 and ignores setGrade; a class without a durability reads
// 1 and ignores setDurability.
template <Item::ItemClass IClass, typename StackPolicy = NoStack, typename OptionPolicy = NoOption,
          typename AttackingStatPolicy = NoAttacking, typename EnchantLevelPolicy = HasEnchantLevel>
class ConcreteItem : public Item {
public:
    // Concrete implementations of the virtual functions
    ItemClass getItemClass() const {
        return IClass;
    }
    string getObjectTableName() const {
        return ItemObjectTableName[getItemClass()];
    }
    ItemInfo* getItemInfo() const {
        return de::gameContext().itemInfos().getItemInfo(getItemClass(), getItemType());
    }

    ItemType_t getItemType() const {
        return m_ItemType;
    }
    void setItemType(ItemType_t itemType) {
        m_ItemType = itemType;
    }

    VolumeWidth_t getVolumeWidth() const {
        return getItemInfo()->getVolumeWidth();
    }
    VolumeHeight_t getVolumeHeight() const {
        return getItemInfo()->getVolumeHeight();
    }
    Weight_t getWeight() const {
        return getItemInfo()->getWeight();
    }

public:
    // Stacking
    bool isStackable() const {
        return m_Stack.hasValue();
    }

    ItemNum_t getNum() const {
        return m_Stack.getValue();
    }
    void setNum(ItemNum_t Num) {
        m_Stack.setValue(Num);
    }

public:
    // Durability
    Durability_t getDurability() const {
        if (!itemClassHasDurability(IClass))
            return 1;
        return m_Durability.getValue();
    }
    void setDurability(Durability_t durability) {
        if (itemClassHasDurability(IClass))
            m_Durability.setValue(durability);
    }
    Durability_t getMaxDurability() const {
        return computeBaseMaxDurability(getItemInfo()->getDurability(), itemClassHasDurability(IClass),
                                        gradeOffsets().durability);
    }

public:
    // Options
    bool hasOptionType() const {
        return m_Option.hasOptionType();
    }
    int getOptionTypeSize() const {
        return m_Option.getOptionTypeSize();
    }
    int getRandomOptionType() const {
        return m_Option.getRandomOptionType();
    }
    const list<OptionType_t>& getOptionTypeList() const {
        return m_Option.getOptionTypeList();
    }
    OptionType_t getFirstOptionType() const {
        return m_Option.getFirstOptionType();
    }
    void removeOptionType(OptionType_t OptionType) {
        m_Option.removeOptionType(OptionType);
    }
    void changeOptionType(OptionType_t currentOptionType, OptionType_t newOptionType) {
        m_Option.changeOptionType(currentOptionType, newOptionType);
    }
    void addOptionType(OptionType_t OptionType) {
        m_Option.addOptionType(OptionType);
    }
    void setOptionType(const list<OptionType_t>& OptionType) {
        m_Option.setOptionType(OptionType);
    }

public:
    // Item grade
    Grade_t getGrade() const {
        if (!itemClassHasGrade(IClass))
            return -1;
        return m_Grade.getValue();
    }
    void setGrade(Grade_t Grade) {
        if (itemClassHasGrade(IClass))
            m_Grade.setValue(Grade);
    }

    Luck_t getLuck() const {
        return (Luck_t)gradeOffsets().luck;
    }

public:
    // Attack attributes
    Damage_t getMinDamage() const {
        return max(1, ((int)getItemInfo()->getMinDamage()) + ((int)getBonusDamage()) + gradeOffsets().damage);
    }
    Damage_t getMaxDamage() const {
        return max(1, ((int)getItemInfo()->getMaxDamage()) + ((int)getBonusDamage()) + gradeOffsets().damage);
    }
    Range_t getRange() const {
        return getItemInfo()->getRange();
    }
    ToHit_t getToHitBonus() const {
        return getItemInfo()->getToHitBonus();
    }
    Speed_t getSpeed() const {
        return getItemInfo()->getSpeed();
    }
    int getCriticalBonus() const {
        return max(0, getItemInfo()->getCriticalBonus() + gradeOffsets().critical);
    }

    BYTE getBulletCount() const {
        return m_AttackingStat.getBulletCount();
    }
    void setBulletCount(BYTE bulletCount) {
        m_AttackingStat.setBulletCount(bulletCount);
    }

    bool isSilverWeapon() const {
        return m_AttackingStat.isSilverWeapon();
    }
    Silver_t getSilver() const {
        return m_AttackingStat.getSilver();
    }
    void setSilver(Silver_t amount) {
        m_AttackingStat.setSilver(amount);
    }

    bool isGun() const {
        return m_AttackingStat.isGun();
    }

    Damage_t getBonusDamage() const {
        return m_AttackingStat.getBonusDamage();
    }
    void setBonusDamage(Damage_t BonusDamage) {
        m_AttackingStat.setBonusDamage(BonusDamage);
    }

public:
    // Defense attributes
    Defense_t getDefenseBonus() const {
        return max(0, ((int)getItemInfo()->getDefenseBonus()) + gradeOffsets().defense);
    }
    Protection_t getProtectionBonus() const {
        return max(0, ((int)getItemInfo()->getProtectionBonus()) + gradeOffsets().protection);
    }

public:
    // Enchant level
    EnchantLevel_t getEnchantLevel() const {
        return m_EnchantLevel.getValue();
    }
    void setEnchantLevel(EnchantLevel_t level) {
        m_EnchantLevel.setValue(level);
    }

private:
    // The offsets the item's grade gives under its class's grade policy;
    // all 0 for a class without grade effects. The grade is read without
    // virtual dispatch, as the grade policies read their own value.
    ItemGradeOffsets gradeOffsets() const {
        return itemClassGradeOffsets(IClass, ConcreteItem::getGrade());
    }

    ItemType_t m_ItemType;

    StackPolicy m_Stack;
    // Read only when the class keeps a durability; left uninitialised, as
    // HasDurability left it, until the item's loader or creator sets it.
    OneValuePolicy<Durability_t> m_Durability;
    OptionPolicy m_Option;
    // Read only when the class keeps a grade; a new item is grade 4.
    OneValuePolicy<Grade_t> m_Grade{4};

    AttackingStatPolicy m_AttackingStat;
    EnchantLevelPolicy m_EnchantLevel;
};

#endif
