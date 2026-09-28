//////////////////////////////////////////////////////////////////////////////
// Filename    : Mace.h
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __MACE_H__
#define __MACE_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class Mace;
//////////////////////////////////////////////////////////////////////////////

class Mace : public ConcreteItem<Item::ITEM_CLASS_MACE, NoStack, HasOption, SlayerWeapon> {
public:
    Mace();
    Mace(ItemType_t itemType, const list<OptionType_t>& optionType);

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
    MP_t getMPBonus() const;

private:
    static Mutex m_Mutex;             // Lock for the item ID registry
    static ItemID_t m_ItemIDRegistry; // Per-class unique item ID generator
};


//////////////////////////////////////////////////////////////////////////////
// class MaceInfo
//////////////////////////////////////////////////////////////////////////////

class MaceInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_MACE;
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

    virtual MP_t getMPBonus() const {
        return m_MPBonus;
    }
    void setMPBonus(MP_t mpBonus) {
        m_MPBonus = mpBonus;
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
    MP_t m_MPBonus;
    Silver_t m_MaxSilver;
    Speed_t m_Speed;
    uint m_ItemLevel;
    int m_CriticalBonus; // Critical chance, which differs per item
};


//////////////////////////////////////////////////////////////////////////////
// class MaceInfoManager;
//////////////////////////////////////////////////////////////////////////////

class MaceInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_MACE;
    }
    virtual void load();
};


//////////////////////////////////////////////////////////////////////////////
// class MaceFactory
//////////////////////////////////////////////////////////////////////////////

class MaceFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_MACE;
    }
    virtual string getItemClassName() const {
        return "Mace";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new Mace(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class MaceLoader;
//////////////////////////////////////////////////////////////////////////////

class MaceLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_MACE;
    }
    virtual string getItemClassName() const {
        return "Mace";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
