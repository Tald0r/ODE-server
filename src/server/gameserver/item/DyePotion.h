//////////////////////////////////////////////////////////////////////////////
// Filename    : DyePotion.h
// Written By  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __DYE_POTION_H__
#define __DYE_POTION_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "Item.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class DyePotion;
//////////////////////////////////////////////////////////////////////////////

class DyePotion : public ConcreteItem<Item::ITEM_CLASS_DYE_POTION, Stackable, NoOption, NoAttacking, NoEnchantLevel> {
public:
    DyePotion();
    DyePotion(ItemType_t itemType, const list<OptionType_t>& optionType, ItemNum_t Num);

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
// class DyePotionInfo
//////////////////////////////////////////////////////////////////////////////

class DyePotionInfo : public ItemInfo {
public:
    enum ITEM_FUNCTION {
        FUNCTION_HAIR = 0,          // Changes the hair color.
        FUNCTION_SKIN = 1,          // Changes the skin color.
        FUNCTION_SEX = 2,           // Changes the sex.
        FUNCTION_BAT = 3,           // Changes the bat color.
        FUNCTION_REGEN = 4,         // Recovery.
        FUNCTION_MASTER_EFFECT = 5, // Changes the master effect color.
    };

public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_DYE_POTION;
    }
    virtual string toString() const;

public:
    virtual BYTE getFunctionFlag() const {
        return m_fFunction;
    }
    virtual void setFunctionFlag(BYTE flag) {
        m_fFunction = flag;
    }

    virtual int getFunctionValue() const {
        return m_FunctionValue;
    }
    virtual void setFunctionValue(int value) {
        m_FunctionValue = value;
    }


    BYTE m_fFunction;    // Function
    int m_FunctionValue; // Value that goes with the function
};

//////////////////////////////////////////////////////////////////////////////
// class DyePotionInfoManager;
//////////////////////////////////////////////////////////////////////////////

class DyePotionInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_DYE_POTION;
    }
    virtual void load();
};


//////////////////////////////////////////////////////////////////////////////
// class DyePotionFactory
//////////////////////////////////////////////////////////////////////////////

class DyePotionFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_DYE_POTION;
    }
    virtual string getItemClassName() const {
        return "DyePotion";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new DyePotion(ItemType, OptionType, 1);
    }
};

//////////////////////////////////////////////////////////////////////////////
// class DyePotionLoader;
//////////////////////////////////////////////////////////////////////////////

class DyePotionLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_DYE_POTION;
    }
    virtual string getItemClassName() const {
        return "DyePotion";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
