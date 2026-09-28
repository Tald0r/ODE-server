//////////////////////////////////////////////////////////////////////////////
// Filename    : SMG.h
// Written By  : Elca
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __SMG_H__
#define __SMG_H__

#include "ConcreteItem.h"
#include "InfoClassManager.h"
#include "ItemFactory.h"
#include "ItemInfo.h"
#include "ItemLoader.h"
#include "ItemPolicies.h"
#include "Mutex.h"

//////////////////////////////////////////////////////////////////////////////
// class SMG;
//////////////////////////////////////////////////////////////////////////////

class SMG : public ConcreteItem<Item::ITEM_CLASS_SMG, NoStack, HasOption, SlayerGun> {
public:
    SMG();
    SMG(ItemType_t itemType, const list<OptionType_t>& optionType);
    ~SMG();

public:
    virtual void create(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y,
                        ItemID_t itemID = 0);
    virtual void save(const string& ownerID, Storage storage, StorageID_t storageID, BYTE x, BYTE y);
    void tinysave(const string& field) const {
        tinysave(field.c_str());
    }
    void tinysave(const char* field) const;
    virtual void saveBullet();
    virtual string toString() const;

    static void initItemIDRegistry(void);

public:
    void makePCItemInfo(PCItemInfo& result) const;

private:
    static Mutex m_Mutex;             // Lock for the item ID registry
    static ItemID_t m_ItemIDRegistry; // Per-class unique item ID generator
};


//////////////////////////////////////////////////////////////////////////////
// class SMGInfo
//////////////////////////////////////////////////////////////////////////////

class SMGInfo : public ItemInfo {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SMG;
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

    ToHit_t getToHitBonus() const {
        return m_ToHitBonus;
    }
    void setToHitBonus(ToHit_t tohit) {
        m_ToHitBonus = tohit;
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
    ToHit_t m_ToHitBonus;
    Speed_t m_Speed;
    uint m_ItemLevel;
    int m_CriticalBonus; // Critical chance, which differs per item
};


//////////////////////////////////////////////////////////////////////////////
// class SMGInfoManager;
//////////////////////////////////////////////////////////////////////////////

class SMGInfoManager : public InfoClassManager {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SMG;
    }
    virtual void load();
};

// global variable declaration


//////////////////////////////////////////////////////////////////////////////
// class SMGFactory
//////////////////////////////////////////////////////////////////////////////

class SMGFactory : public ItemFactory {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SMG;
    }
    virtual string getItemClassName() const {
        return "SMG";
    }

public:
    virtual Item* createItem(ItemType_t ItemType, const list<OptionType_t>& OptionType) {
        return new SMG(ItemType, OptionType);
    }
};


//////////////////////////////////////////////////////////////////////////////
// class SMGLoader;
//////////////////////////////////////////////////////////////////////////////

class SMGLoader : public ItemLoader {
public:
    virtual Item::ItemClass getItemClass() const {
        return Item::ITEM_CLASS_SMG;
    }
    virtual string getItemClassName() const {
        return "SMG";
    }

public:
    virtual void load(Creature* pCreature);
    virtual void load(Zone* pZone);
    virtual void load(StorageID_t storageID, Inventory* pInventory);
};

#endif
