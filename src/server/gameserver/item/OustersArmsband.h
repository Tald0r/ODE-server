//////////////////////////////////////////////////////////////////////////////
// Filename    : OustersArmsband.h
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __OUSTERS_ARMSBAND_H__
#define __OUSTERS_ARMSBAND_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class OustersArmsband;
//////////////////////////////////////////////////////////////////////////////

class OustersArmsband : public ConcreteItem<Item::ITEM_CLASS_OUSTERS_ARMSBAND, NoStack, HasOption, NoAttacking> {
public:
    OustersArmsband();
    OustersArmsband(ItemType_t itemType, const list<OptionType_t>& optionType);
    ~OustersArmsband();

public:
    virtual void create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                        ItemID_t itemID = 0);
    virtual bool destroy();
    virtual void save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y);
    void tinysave(const string& field) const {
        tinysave(field.c_str());
    }
    void tinysave(const char* field) const;
    virtual string toString() const;

    static void initItemIDRegistry(void);

public:
    void setInventory(Inventory* pInventory) {
        m_pInventory = pInventory;
    }
    Inventory* getInventory() const {
        return m_pInventory;
    }

    PocketNum_t getPocketCount(void) const;

    void makePCItemInfo(PCItemInfo& result) const;

private:
    Inventory* m_pInventory; // Inventory

    static Mutex m_Mutex;             // Lock for the item ID registry
    static ItemID_t m_ItemIDRegistry; // Per-class unique item ID generator
};


//////////////////////////////////////////////////////////////////////////////
// class OustersArmsbandInfo
//////////////////////////////////////////////////////////////////////////////

class OustersArmsbandInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_ARMSBAND;
    }

    virtual Durability_t getDurability() const {
        return m_Durability;
    }
    virtual void setDurability(Durability_t durability) {
        m_Durability = durability;
    }

    uint getPocketCount() const {
        return m_PocketCount;
    }
    void setPocketCount(uint pocketCount) {
        m_PocketCount = pocketCount;
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
    uint m_PocketCount;        // Number of pockets
    Defense_t m_DefenseBonus;
    Protection_t m_ProtectionBonus;
    uint m_ItemLevel;
};


//////////////////////////////////////////////////////////////////////////////
// class OustersArmsbandInfoManager;
//////////////////////////////////////////////////////////////////////////////

class OustersArmsbandInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_ARMSBAND;
    }
    virtual void load();
};

// global variable declaration


//////////////////////////////////////////////////////////////////////////////
// class OustersArmsbandFactory
//////////////////////////////////////////////////////////////////////////////

class OustersArmsbandFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_ARMSBAND;
    }
    virtual string getItemClassName() const {
        return "OustersArmsband";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new OustersArmsband(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class OustersArmsbandLoader;
//////////////////////////////////////////////////////////////////////////////

class OustersArmsbandLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_OUSTERS_ARMSBAND;
    }
    virtual string getItemClassName() const {
        return "OustersArmsband";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
