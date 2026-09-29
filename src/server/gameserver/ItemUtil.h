//////////////////////////////////////////////////////////////////////////////
// Filename    : ItemUtil.h
// Written by  : excel96
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __ITEMUTIL_H__
#define __ITEMUTIL_H__

#include <list>
#include <string>

#include "Item.h"

//////////////////////////////////////////////////////////////////////////////
// forward declaration
//////////////////////////////////////////////////////////////////////////////
class Creature;
class Inventory;
class ItemInfo;

// de-core's (domain/EquipRequirement.h), declared here so that the item
// library, which includes this header without linking de-core, needs no
// de-core include path.
namespace decore {
enum class EquipRace;
struct EquipRequirement;
} // namespace decore
class PlayerCreature;
class Slayer;
class Ousters;
class Corpse;

struct ITEM_TEMPLATE;

enum ItemTraceLogType {
    ITEM_LOG_CREATE = 0, // log for item creation
    ITEM_LOG_TRADE,      // log for item trades
    ITEM_LOG_MOVE,       // log for item moves
    ITEM_LOG_DELETE,     // log for item deletion

    ITEM_LOG_MAX
};

enum ItemTraceDetailType {
    DETAIL_COMMAND = 0, // created by a command                        : ITEM_LOG_CREATE
    DETAIL_SHOPBUY,     // bought in a shop                            : ITEM_LOG_CREATE
    DETAIL_PICKUP,      // picked up from the ground                   : ITEM_LOG_MOVE
    DETAIL_DROP,        // dropped on the ground                       : ITEM_LOG_MOVE
    DETAIL_EVENTNPC,    // item created by an event                    : ITEM_LOG_CREATE
    DETAIL_SHOPSELL,    // sold to a shop                              : ITEM_LOG_DELETE
    DETAIL_TIMEOUT,     // vanished by timeout after being dropped     : ITEM_LOG_DELETE
    DETAIL_ENCHANT,     // changed by an enchant                       : ITEM_LOG_DELETE
    DETAIL_OPCLEAR,     // deleted by a command                        : ITEM_LOG_DELETE
    DETAIL_TRADE,       // item trade                                  : ITEM_LOG_TRADE
    DETAIL_MALLBUY,     // item bought on the web                      : ITEM_LOG_CREATE

    DETAIL_MAX
};

enum ITLType {
    ITL_GET = 0, // item GET
    ITL_DROP,    // item DROP
    ITL_ETC,     // item ETC

    ITL_MAX
};

enum ITLDType {
    ITLD_PICKUP = 0, // item PICKUP    GET
    ITLD_TRADE,      // item TRADE     GET/DROP
    ITLD_EVENTNPC,   // item received from an NPC after clearing an event    GET
    ITLD_PETITEM,    // GET
    ITLD_ENCHANT,    // GET/DROP
    ITLD_MIXING,     // GET/DROP
    ITLD_OPTION,     // GET/DROP/ETC

    ITLD_NPCSHOP,     // traded in an NPC shop  GET/DROP
    ITLD_WEBSHOP,     // traded in the web market GET
    ITLD_PRIVATESHOP, // in a private shop GET/DROP

    ITLD_GM,      // game master ACTION GET/DROP/MOVE/ETC
    ITLD_TIMEOUT, // timeout  DROP
    ITLD_DELETE,  // Delete   // there should be no ETC+DELETE case
    ITLD_MOVE,    // Mode 		GET/DROP/ETC

    ITLD_MAX
};

const string ItemTraceLogType2String[] = {
    "CREATE", // 0
    "TRADE",  // 1
    "MOVE",   // 2
    "DELETE", // 3
};

const string ItemTraceLogDetailType2String[] = {
    "COMMAND",  // 0
    "SHOPBUY",  // 1
    "PICKUP",   // 2
    "DROP",     // 3
    "EVENTNPC", // 4
    "SHOPSELL", // 5
    "TIMEOUT",  // 6
    "ENCHANT",  // 7
    "OPCLEAR",  // 8
    "TRADE",    // 9
    "MALLBUY",  // 10
};

const string ITLType2String[] = {
    "GET",  // 0
    "DROP", // 1
    "MOVE", // 2
    "ETC",  // 3
};

const string ITLDType2String[] = {
    "PICKUP",   // 0
    "TRADE",    // 1
    "EVENTNPC", // 2
    "PETITEM",  // 3
    "ENCHANT",  // 4
    "MIXING",   // 5
    "OPTION",   // 6

    "NPCSHOP",     // 7
    "WEBSHOP",     // 8
    "PRIVATESHOP", // 9

    "GM",      // 10
    "TIMEOUT", // 11
    "DELETE",  // 12
    "MOVE"     // 13
};

//////////////////////////////////////////////////////////////////////////////
// Is the item stackable?
//////////////////////////////////////////////////////////////////////////////
bool isStackable(Item::ItemClass IClass);
bool isStackable(const Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Are the items of the same class and type?
//////////////////////////////////////////////////////////////////////////////
bool isSameItem(Item::ItemClass IClass1, Item::ItemClass IClass2, ItemType_t type1, ItemType_t type2);
bool isSameItem(const Item* pItem1, const Item* pItem2);

//////////////////////////////////////////////////////////////////////////////
// Can the two items be stacked?
//////////////////////////////////////////////////////////////////////////////
bool canStack(const Item* pItem1, const Item* pItem2);

//////////////////////////////////////////////////////////////////////////////
// Is it a two-handed weapon?
// Is it a melee weapon?
// Is it a warrior, soldier or cleric weapon?
//////////////////////////////////////////////////////////////////////////////
bool isTwohandWeapon(const Item* pItem);
bool isMeleeWeapon(const Item* pItem);
bool isFighterWeapon(const Item* pItem);
bool isArmsWeapon(const Item* pItem);
bool isClericWeapon(const Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Does the magazine fit the gun?
//////////////////////////////////////////////////////////////////////////////
bool isSuitableMagazine(const Item* pGun, const Item* pMagazine, bool hasVivid);

//////////////////////////////////////////////////////////////////////////////
// Is it a slayer weapon?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerWeapon(Item::ItemClass IClass);
bool isAdvancedSlayerWeapon(Item::ItemClass IClass);
bool isVampireWeapon(Item::ItemClass IClass);
bool isOustersWeapon(Item::ItemClass IClass);

//////////////////////////////////////////////////////////////////////////////
// Is it slayer armor?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerArmor(Item::ItemClass IClass);
bool isVampireArmor(Item::ItemClass IClass);
bool isOustersArmor(Item::ItemClass IClass);

//////////////////////////////////////////////////////////////////////////////
// Is it a slayer accessory?
//////////////////////////////////////////////////////////////////////////////
bool isSlayerAccessory(Item::ItemClass IClass);
bool isVampireAccessory(Item::ItemClass IClass);
bool isOustersAccessory(Item::ItemClass IClass);

//////////////////////////////////////////////////////////////////////////////
// Can the item be repaired?
//////////////////////////////////////////////////////////////////////////////
bool isRepairableItem(const Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Repair the item.
//////////////////////////////////////////////////////////////////////////////
void repairItem(Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Get the item's maximum durability.
//////////////////////////////////////////////////////////////////////////////
Durability_t computeMaxDurability(Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// What pItem requires of a wearer of `race`: its item info's requirement
// raised by each of its options and capped (decore::requiredStats).
//////////////////////////////////////////////////////////////////////////////
decore::EquipRequirement computeEquipRequirement(decore::EquipRace race, const Item* pItem, const ItemInfo* pItemInfo);

//////////////////////////////////////////////////////////////////////////////
// Reload the magazine.
//////////////////////////////////////////////////////////////////////////////
Bullet_t reloadArmsItem(Item* pWeapon, Item* pMagazine);

//////////////////////////////////////////////////////////////////////////////
// Consume a bullet.
//////////////////////////////////////////////////////////////////////////////
Bullet_t decreaseBullet(Item* pWeapon);

//////////////////////////////////////////////////////////////////////////////
// Return the number of bullets left.
//////////////////////////////////////////////////////////////////////////////
Bullet_t getRemainBullet(Item* pWeapon);

//////////////////////////////////////////////////////////////////////////////
// Can the item be picked up?
//////////////////////////////////////////////////////////////////////////////
bool isPortableItem(Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Can the item be used?
// Use here means a consumable item that disappears when used.
//////////////////////////////////////////////////////////////////////////////
bool isUsableItem(Item* pItem, Creature* pUser);

//////////////////////////////////////////////////////////////////////////////
// Decrease the item count.
//////////////////////////////////////////////////////////////////////////////
ItemNum_t decreaseItemNum(Item* pItem, Inventory* pInventory, const string& OwnerID, Storage storage,
                          StorageID_t storageID, BYTE x, BYTE y);

//////////////////////////////////////////////////////////////////////////////
// Logs the bug where items overlap.
//////////////////////////////////////////////////////////////////////////////
void processItemBug(Creature* pCreature, Item* pItem);
void processItemBugEx(Creature* pCreature, Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Create a mysterious item.
//////////////////////////////////////////////////////////////////////////////
Item* getRandomMysteriousItem(Creature* pCreature, Item::ItemClass itemClass, int maxLevel = 0);

//////////////////////////////////////////////////////////////////////////////
// Functions related to options.
//////////////////////////////////////////////////////////////////////////////
// Is a particular OptionType attached?
bool hasOptionType(const list<OptionType_t>& optionTypes, OptionType_t optionType);

// Is a particular OptionClass (STR, DEX...) attached?
bool hasOptionClass(const list<OptionType_t>& optionTypes, OptionType_t optionType);

// When reading from the DB.
void setOptionTypeFromField(list<OptionType_t>& optionTypes, const string& optionField);

// When saving to the DB.
void setOptionTypeToField(const list<OptionType_t>& optionTypes, string& optionField);

// Used with cout.
string getOptionTypeToString(const list<OptionType_t>& optionTypes);

//////////////////////////////////////////////////////////////////////////////
// Check the chance of another option being attached: whether a rare item is created.
//////////////////////////////////////////////////////////////////////////////
bool isPossibleNextOption(ITEM_TEMPLATE* pTemplate);

ItemType_t getUpgradeItemType(Item::ItemClass IClass, ItemType_t itemType, ItemType_t upgradeCount);
ItemType_t getDowngradeItemType(Item::ItemClass IClass, ItemType_t itemType);
bool isPossibleUpgradeItemType(Item::ItemClass IClass);

//////////////////////////////////////////////////////////////////////////////
// For the Christmas tree event.
//////////////////////////////////////////////////////////////////////////////
// Search for tree fragments.
// TPOINT checkEventTree( PlayerCreature* pPC, CoordInven_t iX, CoordInven_t iY );
// TPOINT checkEventDocument( PlayerCreature* pPC, CoordInven_t iX, CoordInven_t iY );
// TPOINT checkEventDoll( PlayerCreature* pPC, CoordInven_t iX, CoordInven_t iY );
TPOINT checkEventPuzzle(PlayerCreature* pPC, CoordInven_t iX, CoordInven_t iY, int start);

// Delete the items in the inventory range (X0, Y0) - (X1, Y1).
void deleteInventoryItem(Inventory* pInventory, CoordInven_t invenX0, CoordInven_t invenY0, CoordInven_t invenX1,
                         CoordInven_t invenY1);

// Put the newbie items into the inventory.
bool addNewbieItemToInventory(Slayer* pSlayer, bool sendPacket = false);
bool addNewbieGoldToInventory(Slayer* pSlayer, bool sendPacket = false);
bool addNewbieItemToGear(Slayer* pSlayer, bool sendPacket = false);

bool addNewbieItemToInventory(Ousters* pOusters, bool sendPacket = false);
bool addNewbieGoldToInventory(Ousters* pOusters, bool sendPacket = false);
bool addNewbieItemToGear(Ousters* pOusters, bool sendPacket = false);

Item::ItemClass getBestNewbieWeaponClass(Slayer* pSlayer);

// Build the option list from an option string.
void makeOptionList(const string& options, list<OptionType_t>& optionList);

void saveDissectionItem(Creature* pCreature, Item* pTreasure, int x, int y);

bool canDecreaseDurability(Item* pItem);

bool canSell(Item* pItem);
bool canPutInStash(Item* pItem);
bool canTrade(Item* pItem);
bool isCoupleRing(Item* pItem);

//////////////////////////////////////////////////////////////////////////////
// Exchange System: Point-only trade item check functions
//////////////////////////////////////////////////////////////////////////////
// Check if item is Blue Sapphire (hard currency)
bool isBlueSapphire(Item* pItem);

// Get base option type by following PreviousType chain
OptionType_t getBaseOptionType(OptionType_t type);

// Check if item has 3 options and at least one is upgraded
bool isUpgradedThreeOptionItem(Item* pItem);

// Check if item can ONLY be traded via exchange (points)
bool isPointOnlyTradeItem(Item* pItem);

bool suitableItemClass(Item::ItemClass iClass, SkillDomainType_t domainType);

// Swap the item for the same-grade item of the matching gender, assuming the female type
// directly follows the male one. After calling this the item type must be saved, or pItem->save() called.
void setItemGender(Item* pItem, GenderRestriction gender);

// Should an item trace log be written for this item?
bool bTraceLog(Item* pItem);

// Writes an item trace log.
void remainTraceLog(Item* pItem, const string& preOwner, const string& owner, ItemTraceLogType logType,
                    ItemTraceDetailType detailType);
void remainTraceLogNew(Item* pItem, const string& owner, ITLType logType, ITLDType detailType, ZoneID_t zid = 0,
                       int x = 0, int y = 0);

// Writes a money trace log.
void remainMoneyTraceLog(const string& preOwner, const string& owner, ItemTraceLogType logType,
                         ItemTraceDetailType detailType, int amount);

// Creates an item bought on the web.
Item* createItemByGoodsID(DWORD goodsID);

// Lottery prize handling (kept here for lack of a better place).
bool bWinPrize(DWORD rewardID, DWORD questLevel);

void deleteFlagEffect(Corpse* pFlagPole, Item* pFlag);

Item* fitToPC(Item* pItem, PlayerCreature* pPC);

#endif
