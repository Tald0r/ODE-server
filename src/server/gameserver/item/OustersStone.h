//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersStone.h
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __OUSTERS_STONE_H__
#define __OUSTERS_STONE_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class OustersStone;
//////////////////////////////////////////////////////////////////////////////

class OustersStone : public ConcreteItem<Item::ITEM_CLASS_OUSTERS_STONE, NoStack, HasOption, NoAttacking> {
public:
    OustersStone();
    OustersStone(ItemType_t itemType, const list<OptionType_t>& optionType);

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
    virtual ElementalType getElementalType() const;
    virtual Elemental_t getElemental() const;

private:
    static Mutex m_Mutex;             // Lock for the item ID registry
    static ItemID_t m_ItemIDRegistry; // Per-class unique item ID generator
};


//////////////////////////////////////////////////////////////////////////////
// class OustersStoneInfo
//////////////////////////////////////////////////////////////////////////////

class OustersStoneInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_STONE;
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

    virtual ElementalType getElementalType() const {
        return m_ElementalType;
    }
    void setElementalType(ElementalType elementalType) {
        m_ElementalType = elementalType;
    }

    virtual Elemental_t getElemental() const {
        return m_Elemental;
    }
    void setElemental(Elemental_t elemental) {
        m_Elemental = elemental;
    }

    virtual string toString() const;

private:
    Durability_t m_Durability; // Durability
    Defense_t m_DefenseBonus;
    Protection_t m_ProtectionBonus;
    uint m_ItemLevel;

    ElementalType m_ElementalType;
    Elemental_t m_Elemental;
};


//////////////////////////////////////////////////////////////////////////////
// class OustersStoneInfoManager;
//////////////////////////////////////////////////////////////////////////////

class OustersStoneInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_STONE;
    }
    virtual void load();
};

// global variable declaration


//////////////////////////////////////////////////////////////////////////////
// class OustersStoneFactory
//////////////////////////////////////////////////////////////////////////////

class OustersStoneFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_STONE;
    }
    virtual string getItemClassName() const {
        return "OustersStone";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new OustersStone(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class OustersStoneLoader;
//////////////////////////////////////////////////////////////////////////////

class OustersStoneLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_STONE;
    }
    virtual string getItemClassName() const {
        return "OustersStone";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
