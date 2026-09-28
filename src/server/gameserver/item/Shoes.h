//////////////////////////////////////////////////////////////////////////////
// Filename    : Shoes.h
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SHOES_H__
#define __SHOES_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class Shoes;
//////////////////////////////////////////////////////////////////////////////

class Shoes : public ConcreteItem<Item::ITEM_CLASS_SHOES, NoStack, HasOption, NoAttacking> {
public:
    Shoes();
    Shoes(ItemType_t itemType, const list<OptionType_t>& optionType);

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
// class ShoesInfo
//////////////////////////////////////////////////////////////////////////////

class ShoesInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SHOES;
    }

    virtual Durability_t getDurability() const {
        return m_Durability;
    }
    virtual void setDurability(Durability_t durability) {
        m_Durability = durability;
    }

    Defense_t getDefenseBonus() const {
        return m_DefenseBonus;
    }
    void setDefenseBonus(Defense_t acBonus) {
        m_DefenseBonus = acBonus;
    }

    Protection_t getProtectionBonus() const {
        return m_ProtectionBonus;
    }
    void setProtectionBonus(Protection_t acBonus) {
        m_ProtectionBonus = acBonus;
    }

    virtual uint getItemLevel(void) const {
        return m_ItemLevel;
    }
    virtual void setItemLevel(uint level) {
        m_ItemLevel = level;
    }

    virtual string toString() const;

private:
    Durability_t m_Durability; // Durability
    Defense_t m_DefenseBonus;  // Defense bonus
    Protection_t m_ProtectionBonus;
    uint m_ItemLevel;
};


//////////////////////////////////////////////////////////////////////////////
// class ShoesInfoManager;
//////////////////////////////////////////////////////////////////////////////

class ShoesInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SHOES;
    }
    virtual void load();
};

// global variable declaration


//////////////////////////////////////////////////////////////////////////////
// class ShoesFactory
//////////////////////////////////////////////////////////////////////////////

class ShoesFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SHOES;
    }
    virtual string getItemClassName() const {
        return "Shoes";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new Shoes(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class ShoesLoader;
//////////////////////////////////////////////////////////////////////////////

class ShoesLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SHOES;
    }
    virtual string getItemClassName() const {
        return "Shoes";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
