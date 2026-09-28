#ifndef __ITEM_POLICIES_H__
#define __ITEM_POLICIES_H__

#include <list>

#include "Types.h"

template <typename T> class OneValuePolicy {
public:
    OneValuePolicy() {}
    OneValuePolicy(T init) : m_Value(init) {}

    bool hasValue() const {
        return true;
    }
    T getValue() const {
        return m_Value;
    }
    void setValue(T value) {
        m_Value = value;
    }

private:
    T m_Value;
};

template <typename T, T initValue> class NoValuePolicy {
public:
    bool hasValue() const {
        return false;
    }
    T getValue() const {
        return initValue;
    }
    void setValue(T value) {}
};

// Stack Policies
typedef NoValuePolicy<ItemNum_t, 1> NoStack;
typedef OneValuePolicy<ItemNum_t> Stackable;

// An item's durability and grade policies are not here: ConcreteItem reads
// them per item class from de-core's table (src/domain/ItemGrade.h).

// Option Policies
class NoOption {
public:
    bool hasOptionType() const {
        return false;
    }
    int getOptionTypeSize() const {
        return 0;
    }
    int getRandomOptionType() const {
        return 0;
    }
    const list<OptionType_t>& getOptionTypeList() const {
        static list<OptionType_t> nullList;
        return nullList;
    }
    OptionType_t getFirstOptionType() const {
        return 0;
    }
    void removeOptionType(OptionType_t OptionType) {}
    void changeOptionType(OptionType_t currentOptionType, OptionType_t newOptionType) {}
    void addOptionType(OptionType_t OptionType) {}
    void setOptionType(const list<OptionType_t>& OptionType) {}
};

class HasOption {
public:
    bool hasOptionType() const {
        return !m_OptionType.empty();
    }
    int getOptionTypeSize() const {
        return m_OptionType.size();
    }
    int getRandomOptionType() const {
        if (m_OptionType.empty())
            return 0;
        int pos = rand() % m_OptionType.size();
        list<OptionType_t>::const_iterator itr = m_OptionType.begin();
        for (int i = 0; i < pos; i++)
            itr++;
        return *itr;
    }

    const list<OptionType_t>& getOptionTypeList() const {
        return m_OptionType;
    }
    OptionType_t getFirstOptionType() const {
        if (m_OptionType.empty())
            return 0;
        return m_OptionType.front();
    }
    void removeOptionType(OptionType_t OptionType) {
        list<OptionType_t>::iterator itr = find(m_OptionType.begin(), m_OptionType.end(), OptionType);
        if (itr != m_OptionType.end())
            m_OptionType.erase(itr);
    }
    void changeOptionType(OptionType_t currentOptionType, OptionType_t newOptionType) {
        list<OptionType_t>::iterator itr = find(m_OptionType.begin(), m_OptionType.end(), currentOptionType);
        if (itr != m_OptionType.end())
            *itr = newOptionType;
    }
    void addOptionType(OptionType_t OptionType) {
        m_OptionType.push_back(OptionType);
    }
    void setOptionType(const list<OptionType_t>& OptionType) {
        m_OptionType = OptionType;
    }

private:
    list<OptionType_t> m_OptionType;
};

// Attacking Stats Policies
class NoAttacking {
public:
    BYTE getBulletCount() const {
        return 0;
    }
    void setBulletCount(BYTE bulletCount) {}

    bool isSilverWeapon() const {
        return false;
    }
    Silver_t getSilver() const {
        return 0;
    }
    void setSilver(Silver_t amount) {}

    bool isGun() const {
        return false;
    }

    Damage_t getBonusDamage() const {
        return 0;
    }
    void setBonusDamage(Damage_t BonusDamage) {}
};

class Weapon {
public:
    Weapon() : m_BonusDamage(0) {}

    BYTE getBulletCount() const {
        return 0;
    }
    void setBulletCount(BYTE bulletCount) {}

    bool isGun() const {
        return false;
    }

    bool isSilverWeapon() const {
        return false;
    }
    Silver_t getSilver() const {
        return 0;
    }
    void setSilver(Silver_t amount) {}

    Damage_t getBonusDamage() const {
        return m_BonusDamage;
    }
    void setBonusDamage(Damage_t BonusDamage) {
        m_BonusDamage = BonusDamage;
    }

private:
    Damage_t m_BonusDamage;
};

class SlayerWeapon : public Weapon {
public:
    SlayerWeapon() : m_Silver(0) {}

    bool isSilverWeapon() const {
        return true;
    }
    Silver_t getSilver() const {
        return m_Silver;
    }
    void setSilver(Silver_t amount) {
        m_Silver = amount;
    }

private:
    Silver_t m_Silver;
};

class SlayerGun : public SlayerWeapon {
public:
    SlayerGun() : m_Bullet(0) {}

    BYTE getBulletCount() const {
        return m_Bullet;
    }
    void setBulletCount(BYTE bulletCount) {
        m_Bullet = bulletCount;
    }

    bool isGun() const {
        return true;
    }

private:
    BYTE m_Bullet;
};

typedef OneValuePolicy<EnchantLevel_t> HasEnchantLevel;
typedef NoValuePolicy<EnchantLevel_t, 0> NoEnchantLevel;

#endif
