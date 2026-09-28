//////////////////////////////////////////////////////////////////////////////
// Filename    : PriceManager.cpp
// Description :
// Class that decides the price when items are bought from or sold to a shop.
// Internally it computes from the original price held by ItemInfoManager.
//////////////////////////////////////////////////////////////////////////////

#include "PriceManager.h"

#include <vector>

#include "Creature.h"
#include "GameContext.h"
#include "GamePlayer.h"
#include "Item.h"
#include "ItemInfoManager.h"
#include "ItemUtil.h"
#include "OptionInfo.h"
#include "Ousters.h"
#include "Slayer.h"
#include "Vampire.h"
#include "VariableManager.h"
#include "domain/ItemPrice.h"
#include "item/OustersSummonItem.h"
#include "item/Skull.h"
#include "item/SlayerPortalItem.h"
#include "item/VampirePortalItem.h"

// The item-class ids the shared price rules branch on must be this
// server's.
static_assert(decore::itemclass::Potion == Item::ITEM_CLASS_POTION);
static_assert(decore::itemclass::Skull == Item::ITEM_CLASS_SKULL);
static_assert(decore::itemclass::Serum == Item::ITEM_CLASS_SERUM);
static_assert(decore::itemclass::SlayerPortalItem == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM);
static_assert(decore::itemclass::VampirePortalItem == Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM);
static_assert(decore::itemclass::Larva == Item::ITEM_CLASS_LARVA);
static_assert(decore::itemclass::Pupa == Item::ITEM_CLASS_PUPA);
static_assert(decore::itemclass::ComposMei == Item::ITEM_CLASS_COMPOS_MEI);
static_assert(decore::itemclass::OustersSummonItem == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM);
static_assert(decore::itemclass::MoonCard == Item::ITEM_CLASS_MOON_CARD);

namespace {

// The inputs getPrice and getRepairPrice share: the item, its table price,
// its options' price multipliers (stored in `multipliers`, which the input
// points into) and its durability. The charges are left at 0; each caller
// reads the ones its rule uses.
decore::ItemPriceInput gatherItemPriceInput(Item* pItem, std::vector<int>& multipliers) {
    decore::ItemPriceInput input = {};
    input.itemClass = pItem->getItemClass();
    input.itemType = pItem->getItemType();

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());
    input.basePrice = pItemInfo->getPrice();
    input.grade = pItem->getGrade();

    const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();
    list<OptionType_t>::const_iterator itr;
    for (itr = optionTypes.begin(); itr != optionTypes.end(); itr++) {
        OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(*itr);
        Assert(pOptionInfo != NULL);
        multipliers.push_back(pOptionInfo->getPriceMultiplier());
    }
    input.optionPriceMultipliers = multipliers.data();
    input.optionCount = (int)multipliers.size();

    input.maxDurability = computeMaxDurability(pItem);
    input.curDurability = pItem->getDurability();
    return input;
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
// getPrice()
// Determines the actual price of an item from its item info.
// The nDiscount parameter (a percentage) controls the price.
// The rule is decore::itemPrice; this gathers its inputs. The item, option
// and durability inputs are read for every item, including the ones priced
// flat (given away, time-limited, the crown moon card); every item a shop
// handles has all three. The creature is read only when it is not NULL.
//////////////////////////////////////////////////////////////////////////////
Price_t PriceManager::getPrice(Item* pItem, MarketCond_t nDiscount, ShopRackType_t shopType,
                               Creature* pCreature) const {
    std::vector<int> multipliers;
    decore::ItemPriceInput input = gatherItemPriceInput(pItem, multipliers);

    input.createTypeGame = (pItem->getCreateType() == Item::CREATE_TYPE_GAME);
    input.timeLimited = pItem->isTimeLimitItem();
    input.crownPrice = de::gameContext().variables().getVariable(CROWN_PRICE);

    // A slayer portal, a vampire portal and an ousters summon item are
    // priced with their current charges.
    if (pItem->getItemClass() == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM) {
        input.charge = dynamic_cast<SlayerPortalItem*>(pItem)->getCharge();
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_VAMPIRE_PORTAL_ITEM) {
        input.charge = dynamic_cast<VampirePortalItem*>(pItem)->getCharge();
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM) {
        input.charge = dynamic_cast<OustersSummonItem*>(pItem)->getCharge();
    }

    input.marketCond = nDiscount;
    input.mysteriousRack = (shopType == SHOP_RACK_MYSTERIOUS);

    input.race = decore::PriceRace::None;
    if (pCreature != NULL) {
        if (pCreature->isSlayer()) {
            Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);
            Attr_t CSTR = pSlayer->getSTR(ATTR_CURRENT);
            Attr_t CDEX = pSlayer->getDEX(ATTR_CURRENT);
            Attr_t CINT = pSlayer->getINT(ATTR_CURRENT);
            input.race = decore::PriceRace::Slayer;
            input.currentStatSum = CSTR + CDEX + CINT;
        } else if (pCreature->isVampire()) {
            input.race = decore::PriceRace::Vampire;
        } else if (pCreature->isOusters()) {
            input.race = decore::PriceRace::Ousters;
        }
    }

    // The premium half-price event applies to a paying user; the Blood Bible
    // ratio is the player's own.
    PlayerCreature* pPC = NULL;
    if (pCreature != NULL && pCreature->isPC())
        pPC = dynamic_cast<PlayerCreature*>(pCreature);

    if (de::gameContext().variables().getVariable(PREMIUM_HALF_EVENT) && pPC != NULL) {
        GamePlayer* pGamePlayer = dynamic_cast<GamePlayer*>(pPC->getPlayer());
        input.premiumHalf = (pGamePlayer != NULL && pGamePlayer->isPayPlaying());
    }
    if (pPC != NULL)
        input.potionPriceRatio = pPC->getPotionPriceRatio();

    return decore::itemPrice(input);
}

//////////////////////////////////////////////////////////////////////////////
// getRepairPrice()
// Returns the cost of repairing an item.
// For a completely ruined item the repair cost is
// one tenth of the item's original price.
// The rule is decore::repairPrice; this gathers its inputs, the item, option
// and durability ones for a portal or summon item too.
//////////////////////////////////////////////////////////////////////////////
Price_t PriceManager::getRepairPrice(Item* pItem, Creature* pCreature) const {
    std::vector<int> multipliers;
    decore::ItemPriceInput input = gatherItemPriceInput(pItem, multipliers);

    // A slayer portal and an ousters summon item are charged for the
    // charges they lack.
    if (pItem->getItemClass() == Item::ITEM_CLASS_SLAYER_PORTAL_ITEM) {
        SlayerPortalItem* pSlayerPortalItem = dynamic_cast<SlayerPortalItem*>(pItem);
        input.maxCharge = pSlayerPortalItem->getMaxCharge();
        input.charge = pSlayerPortalItem->getCharge();
    } else if (pItem->getItemClass() == Item::ITEM_CLASS_OUSTERS_SUMMON_ITEM) {
        OustersSummonItem* pOustersSummonItem = dynamic_cast<OustersSummonItem*>(pItem);
        input.maxCharge = pOustersSummonItem->getMaxCharge();
        input.charge = pOustersSummonItem->getCharge();
    }

    return decore::repairPrice(input);
}

//////////////////////////////////////////////////////////////////////////////
// getSilverCoatingPrice()
// The price of silver-coating an item.
//////////////////////////////////////////////////////////////////////////////
Price_t PriceManager::getSilverCoatingPrice(Item* pItem, Creature* pCreature) const {
    if (pItem == NULL)
        return 0;

    switch (pItem->getItemClass()) {
    case Item::ITEM_CLASS_BLADE:
    case Item::ITEM_CLASS_SWORD:
    case Item::ITEM_CLASS_CROSS:
    case Item::ITEM_CLASS_MACE:
        break;
    default:
        return 0;
    }

    ItemInfo* pItemInfo = de::gameContext().itemInfos().getItemInfo(pItem->getItemClass(), pItem->getItemType());
    double maxSilver = pItemInfo->getMaxSilver();
    double finalPrice = 0;

    // Stopgap: the price is the amount of silver.
    finalPrice = maxSilver;

    return max(0, (int)finalPrice);
}

//////////////////////////////////////////////////////////////////////////////
// getStashPrice()
// Returns the price of a stash slot.
// It changes rarely, so the values are written into the code.
//////////////////////////////////////////////////////////////////////////////
Price_t PriceManager::getStashPrice(BYTE index, Creature* pCreature) const {
    Price_t price = 0;

    switch (index) {
    case 1:
        price = 100000;
        break;
    case 2:
        price = 1000000;
        break;
    case 3:
        price = 10000000;
        break;
    default:
        cerr << "PriceManager::getStashPrice() : Unknown Stash Index" << endl;
        Assert(false);
    }

    if (pCreature != NULL) {
        if (pCreature->isSlayer()) {
        } else if (pCreature->isVampire()) {
        } else if (pCreature->isOusters()) {
        }
    }

    return price;
}


//////////////////////////////////////////////////////////////////////////////
// Price function for events.
// Information for the star item used in the Christmas event.
// The same event is reused for Children's Day, so the code is enabled again.
//
// Since the star event may run again,
// renaming this to STAR_EVENT_CODE should be considered.
//////////////////////////////////////////////////////////////////////////////
int PriceManager::getStarPrice(Item* pItem, XMAS_STAR& star) const {
    Assert(pItem != NULL);

    ItemType_t IType = pItem->getItemType();
    OptionType_t OType = pItem->getFirstOptionType();

    Assert(OType != 0);

    OptionInfo* pOptionInfo = de::gameContext().optionInfos().getOptionInfo(OType);
    Assert(pOptionInfo != NULL);
    OptionClass OClass = pOptionInfo->getClass();

    switch (OClass) {
    case OPTION_DAMAGE:
        star.color = STAR_COLOR_BLACK;
        break;
    case OPTION_STR:
        star.color = STAR_COLOR_RED;
        break;
    case OPTION_INT:
        star.color = STAR_COLOR_BLUE;
        break;
    case OPTION_DEX:
        star.color = STAR_COLOR_GREEN;
        break;
    case OPTION_ATTACK_SPEED:
        star.color = STAR_COLOR_CYAN;
        break;
    default:
        Assert(false);
        break;
    }

    star.amount = (IType - 1) * 20;

    return 0;
}

int PriceManager::getBallPrice(int price, XMAS_STAR& star) const {
    star.amount = price;
    star.color = STAR_COLOR_PINK;

    return 0;
}

// Mysterious item price.
// The price varies with itemClass and with pCreature's attributes.
Price_t PriceManager::getMysteriousPrice(Item::ItemClass itemClass, Creature* pCreature) const {
    int multiplier = 1;

    if (pCreature->isSlayer()) {
        Slayer* pSlayer = dynamic_cast<Slayer*>(pCreature);

        Attr_t CSTR = pSlayer->getSTR(ATTR_BASIC);
        Attr_t CDEX = pSlayer->getDEX(ATTR_BASIC);
        Attr_t CINT = pSlayer->getINT(ATTR_BASIC);
        Attr_t CSUM = CSTR + CDEX + CINT;

        // Between 0 and 20
        multiplier = CSUM / 15;
    } else if (pCreature->isVampire()) {
        Vampire* pVampire = dynamic_cast<Vampire*>(pCreature);

        Level_t CLevel = pVampire->getLevel();

        // Between 0 and 20
        multiplier = CLevel / 5;
    } else if (pCreature->isOusters()) {
        Ousters* pOusters = dynamic_cast<Ousters*>(pCreature);

        Level_t CLevel = pOusters->getLevel();

        // Between 0 and 20
        multiplier = CLevel / 5;
    }

    // Between 1 and 20
    multiplier = max(1, multiplier);

    // Get the average price.
    InfoClassManager* pInfoClass = de::gameContext().itemInfos().getInfoManager(itemClass);
    Assert(pInfoClass != NULL);

    // Average price * attribute ratio.
    int finalPrice = (int)pInfoClass->getAveragePrice() * multiplier;

    // Apply the Blood Bible bonus.
    if (pCreature->isPC()) {
        PlayerCreature* pPC = dynamic_cast<PlayerCreature*>(pCreature);
        int ratio = pPC->getGamblePriceRatio();
        if (ratio != 0) {
            // The ratio value is negative.
            finalPrice += getPercentValue(finalPrice, ratio);
        }
    }

    return finalPrice;
}
