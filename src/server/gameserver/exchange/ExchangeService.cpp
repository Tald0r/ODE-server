//////////////////////////////////////////////////////////////////////////////
// Filename : ExchangeService.cpp
// Written by : Exchange System
// Description : Core business logic service for Exchange System
//////////////////////////////////////////////////////////////////////////////

#include "ExchangeService.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <iostream>

#include "ConnectionSettings.h"
#include "GCExchangeList.h" // For ExchangeListing definition
#include "Inventory.h"
#include "Item.h"
#include "ItemUtil.h"
#include "KernelContext.h"
#include "PlayerCreature.h"
#include "Properties.h"
#include "Utility.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////
// Static members
//////////////////////////////////////////////////////////////////////////////

uint8_t ExchangeService::m_TaxRate = 8;         // Default 8% tax
int ExchangeService::m_ListingDurationDays = 3; // Default 3 days

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

namespace {
string _getCurrentTime() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return string(buf);
}

string _addHoursToNow(int hours) {
    time_t now = time(NULL);
    now += hours * 3600;
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return string(buf);
}

// The server id listings, orders and buy requests are scoped by. Every game
// server answers the same value, so they share one market, and
// CGExchangeListHandler browses with it too. It is not the configured
// ServerID (the shipped configurations set 0) and does not identify a game
// server; the idempotency key reads the configured ids itself.
int16_t _marketServerID() {
    return 1;
}

// The key a buy is recorded under, from the client's key and this game
// server's configured WorldID and ServerID.
string _idempotencyKeyFor(const string& clientKey, int64_t listingID) {
    const Properties& config = de::kernelContext().config();
    return resolveExchangeIdempotencyKey(clientKey, config.getPropertyInt("WorldID"), config.getPropertyInt("ServerID"),
                                         listingID);
}

// Check if player has inventory space
bool _checkInventorySpace(PlayerCreature* pPlayer) {
    if (!pPlayer)
        return false;

    Inventory* pInv = pPlayer->getInventory();
    if (!pInv)
        return false;

    // Check if inventory has at least one empty slot
    // This is a simplified check - should be more thorough
    return pInv->getItemNum() < (pInv->getWidth() * pInv->getHeight());
}
} // namespace

//////////////////////////////////////////////////////////////////////////////
// Browse operations
//////////////////////////////////////////////////////////////////////////////

vector<ExchangeListing> ExchangeService::getListings(int16_t serverID, int page, int pageSize,
                                                     const ExchangeListingFilter& filter) {
    // The page is read first and filtered in memory, so a filtered page holds
    // the matching listings of that page, not a page of matching listings.
    vector<ExchangeListing> allListings =
        defaultExchangeRepository().getListings(serverID, LISTING_STATUS_ACTIVE, page, pageSize);

    vector<ExchangeListing> filtered;
    for (const auto& listing : allListings) {
        if (matchesExchangeListingFilter(listing, filter))
            filtered.push_back(listing);
    }

    return filtered;
}

int ExchangeService::getListingsCount(int16_t serverID, const ExchangeListingFilter& filter) {
    // For accurate count, we need a DB query
    // For now, return a placeholder
    vector<ExchangeListing> listings = getListings(serverID, 1, 1000, // Get max results
                                                   filter);
    return listings.size();
}

unique_ptr<ExchangeListing> ExchangeService::getListing(int64_t listingID) {
    return unique_ptr<ExchangeListing>(defaultExchangeRepository().getListing(listingID));
}

//////////////////////////////////////////////////////////////////////////////
// Listing operations
//////////////////////////////////////////////////////////////////////////////

Outcome<ExchangeListingCreated, ExchangeRejection> ExchangeService::createListing(PlayerCreature* pSeller, Item* pItem,
                                                                                  int pricePoint, int durationHours) {
    typedef Outcome<ExchangeListingCreated, ExchangeRejection> Result;

    // Validate inputs
    if (!pSeller) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_NOT_FOUND));
    }
    if (!pItem) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_NOT_FOUND));
    }
    Outcome<void, ExchangeRejection> price = validateListingPrice(pricePoint);
    if (price.isRejected()) {
        return Result::Rejected(std::move(price).rejection());
    }

    // Get player info
    string account = pSeller->getName(); // Using name as account identifier
    string playerName = pSeller->getName();
    uint8_t race = 0; // TODO: Get actual race from PlayerCreature

    // Verify item ownership
    Inventory* pInv = pSeller->getInventory();
    if (!pInv || !pInv->hasItem(pItem->getObjectID())) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_OWNERSHIP));
    }

    // Check if item is tradeable
    if (!canTrade(pItem)) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_TRADEABLE));
    }

    // Check exchange storage has space (conceptually - items stored by DB)
    // In this implementation, items remain in DB with STORAGE_EXCHANGE flag

    // Calculate expiration
    string createdAt = _getCurrentTime();
    string expireAt = _addHoursToNow(durationHours);

    // Create listing record
    ExchangeListing listing;
    listing.listingID = 0; // Will be set by DB auto-increment
    listing.serverID = _marketServerID();
    listing.sellerAccount = account;
    listing.sellerPlayer = playerName;
    listing.sellerRace = race;
    listing.itemClass = pItem->getItemClass();
    listing.itemType = pItem->getItemType();
    listing.itemID = 0; // TODO: Get ItemID from Item
    listing.objectID = pItem->getObjectID();
    listing.pricePoint = pricePoint;
    listing.currency = 0; // 0 = points
    listing.status = LISTING_STATUS_ACTIVE;
    listing.buyerAccount = "";
    listing.buyerPlayer = "";
    listing.taxRate = m_TaxRate;
    listing.taxAmount = 0;
    listing.createdAt = createdAt;
    listing.expireAt = expireAt;
    listing.soldAt = "";
    listing.cancelledAt = "";
    listing.updatedAt = createdAt;
    listing.version = 0;

    // Create item snapshot
    createItemSnapshot(pItem, listing);

    // Save to database
    int64_t listingID = defaultExchangeRepository().createListing(listing);
    if (listingID <= 0) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_DATABASE_ERROR));
    }

    // Move item to exchange storage
    // Note: The item will be saved with STORAGE_EXCHANGE type
    if (!moveItemToExchangeStorage(pSeller, pItem)) {
        // Rollback listing creation
        defaultExchangeRepository().cancelListing(listingID);
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_STORAGE_FULL));
    }

    ExchangeListingCreated created;
    created.listingID = listingID;
    return Result::Ok(created);
}

Outcome<void, ExchangeRejection> ExchangeService::cancelListing(PlayerCreature* pSeller, int64_t listingID) {
    typedef Outcome<void, ExchangeRejection> Result;

    if (!pSeller) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_NOT_FOUND));
    }

    Result decision = decideCancelListing(defaultExchangeRepository(), pSeller->getName(), listingID);
    if (decision.isRejected()) {
        return decision;
    }

    // Mark as cancelled in DB
    if (!defaultExchangeRepository().cancelListing(listingID)) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_DATABASE_ERROR));
    }

    // Note: Item remains in exchange storage until claimed
    // Seller needs to claim the item back

    return Result::Ok();
}

vector<ExchangeListing> ExchangeService::getSellerListings(const string& sellerAccount, uint8_t status) {
    return defaultExchangeRepository().getSellerListings(sellerAccount, status);
}

//////////////////////////////////////////////////////////////////////////////
// Buying operations
//////////////////////////////////////////////////////////////////////////////

Outcome<ExchangePurchase, ExchangeRejection> ExchangeService::buyListing(PlayerCreature* pBuyer, int64_t listingID,
                                                                         const string& idempotencyKey) {
    typedef Outcome<ExchangePurchase, ExchangeRejection> Result;

    if (!pBuyer) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_NOT_FOUND));
    }

    // Account and player are the same name here; the ledger is keyed by the
    // account one and the listing carries the player one.
    ExchangeBuyRequest request;
    request.listingID = listingID;
    request.buyerAccount = pBuyer->getName();
    request.buyerPlayer = pBuyer->getName();
    request.idempotencyKey = _idempotencyKeyFor(idempotencyKey, listingID);
    request.serverID = _marketServerID();
    request.taxRate = m_TaxRate;

    // Check expiration
    // (DB should handle this, but double-check)
    // TODO: Parse expireAt and compare with current time

    Outcome<ExchangePurchaseTerms, ExchangeRejection> decision = decideBuyListing(defaultExchangeRepository(), request);
    if (decision.isRejected()) {
        return Result::Rejected(std::move(decision).rejection());
    }
    const ExchangePurchaseTerms terms = std::move(decision).events();

    return completeExchangePurchase(defaultExchangeRepository(), request, terms, _marketServerID());
}

vector<ExchangeOrder> ExchangeService::getBuyerOrders(const string& buyerPlayer, uint8_t status) {
    return defaultExchangeRepository().getBuyerOrders(buyerPlayer, status);
}

vector<ExchangeOrder> ExchangeService::getSellerOrders(const string& sellerPlayer, uint8_t status) {
    return defaultExchangeRepository().getSellerOrders(sellerPlayer, status);
}

//////////////////////////////////////////////////////////////////////////////
// Claim operations
//////////////////////////////////////////////////////////////////////////////

vector<ExchangeClaim> ExchangeService::prepareClaimList(PlayerCreature* pPlayer) {
    vector<ExchangeClaim> claims;

    if (!pPlayer)
        return claims;

    string playerName = pPlayer->getName();

    // Get buyer's paid orders (ready to deliver)
    vector<ExchangeOrder> orders = getBuyerOrders(playerName, ORDER_STATUS_PAID);
    for (const auto& order : orders) {
        unique_ptr<ExchangeListing> pListing = getListing(order.listingID);
        if (pListing) {
            ExchangeClaim claim;
            claim.id = order.orderID;
            claim.itemName = pListing->itemName;
            claim.pricePoint = order.pricePoint;
            claim.type = 0; // Buyer claim
            claim.status = order.status;
            claims.push_back(claim);
        }
    }

    // Get seller's cancelled/expired listings (ready to return)
    vector<ExchangeListing> cancelledListings = getSellerListings(playerName, LISTING_STATUS_CANCELLED);
    for (const auto& listing : cancelledListings) {
        ExchangeClaim claim;
        claim.id = listing.listingID;
        claim.itemName = listing.itemName;
        claim.pricePoint = listing.pricePoint;
        claim.type = 1; // Seller claim
        claim.status = listing.status;
        claims.push_back(claim);
    }

    return claims;
}

Outcome<void, ExchangeRejection> ExchangeService::claimItem(PlayerCreature* pPlayer, int64_t orderOrListingID,
                                                            bool isBuyerClaim) {
    typedef Outcome<void, ExchangeRejection> Result;

    if (!pPlayer) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_ITEM_NOT_FOUND));
    }

    // Check inventory space
    if (!checkInventorySpace(pPlayer)) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_INVENTORY_FULL));
    }

    if (isBuyerClaim) {
        // Buyer claiming purchased item
        Outcome<ExchangeBuyerClaim, ExchangeRejection> decision =
            decideBuyerClaim(defaultExchangeRepository(), pPlayer->getName(), orderOrListingID);
        if (decision.isRejected()) {
            return Result::Rejected(std::move(decision).rejection());
        }

        // Load item from exchange storage
        // Note: This requires loading the item by ObjectID from DB
        // For now, we'll mark the order as delivered
        // The actual item transfer should be handled by the item manager

        if (!defaultExchangeRepository().markOrderDelivered(orderOrListingID)) {
            return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_DATABASE_ERROR));
        }

        // TODO: Actually transfer item to player's inventory
        // This requires finding the item by ObjectID and moving it

        return Result::Ok();
    } else {
        // Seller claiming back cancelled/expired item
        Result decision = decideSellerClaim(defaultExchangeRepository(), pPlayer->getName(), orderOrListingID);
        if (decision.isRejected()) {
            return decision;
        }

        // TODO: Load item from exchange storage and add to inventory
        // This requires item manager integration

        return Result::Ok();
    }
}

//////////////////////////////////////////////////////////////////////////////
// Point operations
//////////////////////////////////////////////////////////////////////////////

int ExchangeService::getPointBalance(const string& account) {
    return defaultExchangeRepository().getPointBalance(account);
}

Outcome<ExchangePointsAdjusted, ExchangeRejection> ExchangeService::adjustPoints(const string& account, int delta,
                                                                                 uint8_t reason, int64_t refListingID,
                                                                                 int64_t refOrderID,
                                                                                 const string& idempotencyKey) {
    typedef Outcome<ExchangePointsAdjusted, ExchangeRejection> Result;

    ExchangePointsAdjusted adjusted;
    if (!defaultExchangeRepository().adjustPoints(account, delta, adjusted.balanceAfter, reason, refListingID,
                                                  refOrderID, idempotencyKey)) {
        return Result::Rejected(ExchangeRejection(EXCHANGE_FAIL_UNKNOWN, "duplicate ledger key or balance below zero"));
    }

    return Result::Ok(adjusted);
}

//////////////////////////////////////////////////////////////////////////////
// Startup
//////////////////////////////////////////////////////////////////////////////

bool ExchangeService::openPointLedger() {
    const Properties& config = de::kernelContext().config();
    const de::ConnectionSettings game = de::connectionSettings(config, "DB");
    const de::ConnectionSettings account = de::connectionSettings(config, "UI_DB");

    string failure;
    if (openExchangePointLedger(account.db, failure)) {
        cout << "Exchange point ledger: `" << account.db << "`.AccountPoint and PointLedger on the game connection"
             << endl;
        return true;
    }

    // A port of 0 is the driver's default.
    auto port = [](uint value) { return value == 0 ? string("default port") : "port " + std::to_string(value); };
    const string message =
        "Exchange purchases are disabled. The point tables are read from the game connection (DB_* block: " +
        game.user + "@" + game.host + ", " + port(game.port) + ", schema " + game.db +
        ") by the account schema's name (UI_DB_* block: UI_DB_DB = " + account.db + " on " + account.host + ", " +
        port(account.port) +
        "), so both blocks must name one MySQL server and DB_USER must be able to "
        "read and write the account schema's AccountPoint and PointLedger. The check failed: " +
        failure;
    cerr << message << endl;
    diagnosticFilelog("DBError.log", "%s", message.c_str());
    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Maintenance operations
//////////////////////////////////////////////////////////////////////////////

void ExchangeService::scanExpiredListings() {
    __BEGIN_TRY

    // Scan for expired active listings
    // In production, use indexed query on ExpireAt column
    vector<ExchangeListing> expiredListings = defaultExchangeRepository().getExpiredListings();

    filelog("ExchangeService.log", "Scanning for expired listings, found %d expired", expiredListings.size());

    for (const auto& listing : expiredListings) {
        filelog("ExchangeService.log", "Expiring listing ID: %lld, Item: %s, Seller: %s", listing.listingID,
                listing.itemName.c_str(), listing.sellerPlayer.c_str());

        // Mark listing as expired
        // This will:
        // 1. Set listing status to EXPIRED
        // 2. Allow seller to reclaim the item
        defaultExchangeRepository().expireListing(listing.listingID);
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// Helper methods
//////////////////////////////////////////////////////////////////////////////

string ExchangeService::getCurrentTimestamp() {
    return _getCurrentTime();
}

bool ExchangeService::moveItemToExchangeStorage(PlayerCreature* pPlayer, Item* pItem) {
    if (!pPlayer || !pItem)
        return false;

    // Get the item's current storage location
    int storage, x, y;
    pPlayer->findItemOID(pItem->getObjectID(), storage, x, y);

    // Save item with STORAGE_EXCHANGE type
    // The item will be associated with the exchange system
    string owner = pPlayer->getName();


    // Remove from inventory
    Inventory* pInv = pPlayer->getInventory();
    if (pInv) {
        // pInv->deleteItem(x, y);  // Remove from current slot
    }

    return true;
}

bool ExchangeService::moveItemFromExchangeStorage(PlayerCreature* pPlayer, int64_t listingID, Item* pItem) {
    if (!pPlayer || !pItem)
        return false;

    // Add item to player's inventory
    Inventory* pInv = pPlayer->getInventory();
    if (!pInv)
        return false;


    return true;
}

void ExchangeService::createItemSnapshot(Item* pItem, ExchangeListing& listing) {
    if (!pItem)
        return;

    // Set basic item info
    listing.itemName = pItem->toString(); // Or get name from item info
    listing.enchantLevel = 0;             // TODO: Get from pItem
    listing.grade = 0;                    // TODO: Get from pItem
    listing.durability = 0;               // TODO: Get from pItem
    listing.silver = 0;                   // TODO: Get from pItem
    listing.stackCount = 1;               // TODO: Get from pItem

    // Get option info
    if (pItem->hasOptionType()) {
        const list<OptionType_t>& optionTypes = pItem->getOptionTypeList();
        int idx = 0;
        for (OptionType_t type : optionTypes) {
            (void)type; // Will be used when setting option fields
            if (idx >= 3)
                break;

            idx++;
        }
    }
}

int16_t ExchangeService::getMarketServerID() {
    return _marketServerID();
}

bool ExchangeService::checkInventorySpace(PlayerCreature* pPlayer) {
    return _checkInventorySpace(pPlayer);
}
