//////////////////////////////////////////////////////////////////////////////
// Filename    : Sword.h
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SWORD_H__
#define __SWORD_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class Sword;
//////////////////////////////////////////////////////////////////////////////

class Sword : public ConcreteItem<Item::ITEM_CLASS_SWORD, NoStack, HasOption, SlayerWeapon> {
public:
    Sword();
    Sword(ItemType_t itemType, const list<OptionType_t>& optionType);

public:
    virtual void create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                        ItemID_t itemID = 0);
    virtual void save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y);
    void tinysave(const string& field) const {
        tinysave(field.c_str());
    }
    void tinysave(const char* field) const;
    virtual string toString() const;

    static void initItemIDRegistry(void);

public:
private:
    static Mutex m_Mutex;             // Lock for the item ID registry
    static ItemID_t m_ItemIDRegistry; // Per-class unique item ID generator
};


//////////////////////////////////////////////////////////////////////////////
// class SwordInfo
//////////////////////////////////////////////////////////////////////////////

class SwordInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SWORD;
    }

    virtual Durability_t getDurability() const {
        return m_Durability;
    }
    virtual void setDurability(Durability_t durability) {
        m_Durability = durability;
    }

    virtual Damage_t getMinDamage() const {
        return m_MinDamage;
    }
    virtual void setMinDamage(Damage_t minDamage) {
        m_MinDamage = minDamage;
    }

    virtual Damage_t getMaxDamage() const {
        return m_MaxDamage;
    }
    virtual void setMaxDamage(Damage_t maxDamage) {
        m_MaxDamage = maxDamage;
    }

    Range_t getRange() const {
        return m_Range;
    }
    void setRange(Range_t range) {
        m_Range = range;
    }


    virtual Silver_t getMaxSilver() const {
        return m_MaxSilver;
    }
    virtual void setMaxSilver(Silver_t amount) {
        m_MaxSilver = amount;
    }

    virtual Speed_t getSpeed(void) const {
        return m_Speed;
    }
    virtual void setSpeed(Speed_t speed) {
        m_Speed = speed;
    }

    virtual uint getItemLevel(void) const {
        return m_ItemLevel;
    }
    virtual void setItemLevel(uint level) {
        m_ItemLevel = level;
    }

    virtual int getCriticalBonus(void) const {
        return m_CriticalBonus;
    }
    virtual void setCriticalBonus(int bonus) {
        m_CriticalBonus = bonus;
    }

    virtual string toString() const;

private:
    Durability_t m_Durability;
    Damage_t m_MinDamage;
    Damage_t m_MaxDamage;
    Range_t m_Range;
    Silver_t m_MaxSilver;
    Speed_t m_Speed;
    uint m_ItemLevel;
    int m_CriticalBonus; // Critical chance, which differs per item
};


//////////////////////////////////////////////////////////////////////////////
// class SwordInfoManager;
//////////////////////////////////////////////////////////////////////////////

class SwordInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SWORD;
    }
    virtual void load();
};


//////////////////////////////////////////////////////////////////////////////
// class SwordFactory
//////////////////////////////////////////////////////////////////////////////

class SwordFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SWORD;
    }
    virtual string getItemClassName() const {
        return "Sword";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new Sword(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class SwordLoader;
//////////////////////////////////////////////////////////////////////////////

class SwordLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SWORD;
    }
    virtual string getItemClassName() const {
        return "Sword";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
