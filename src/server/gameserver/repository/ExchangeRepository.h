#ifndef __EXCHANGE_REPOSITORY_H__
#define __EXCHANGE_REPOSITORY_H__

#include <cstdint>
#include <string>
#include <vector>

#include "Types.h"

// The Exchange feature's tables: the listings and the orders placed on
// them (ExchangeListing, ExchangeOrder) and the account point ledger
// (AccountPoint, PointLedger), plus the transaction pair the buy path
// wraps around a purchase.
//
// One connection. Every statement runs on the thread's game connection
// (DatabaseManager::getConnection, the DB_* block). The listings and orders
// live in that connection's own schema; AccountPoint and PointLedger live
// in the account database (initdb/USERINFO.sql), and the point statements
// name them by that schema -- `<schema>`.AccountPoint -- instead of asking
// for a connection of their own. The account connection
// (getUserInfoConnection()) is one per process, shared by every thread
// without a lock, so a zone thread's purchase cannot run on it; by schema
// name the ledger runs on the thread's own connection, and a purchase is
// one transaction on one connection whose commit is atomic.
//
// The point ledger opens at startup. openExchangePointLedger (below) names
// the account schema -- the gameserver passes the configuration's UI_DB_DB --
// and checks, on the calling thread's game connection, that the game and
// account connections reach one MySQL server (their @@server_uuid) and that
// every statement shape the ledger issues runs there against no row: the
// tables, their columns and DB_USER's privileges on them. Until a check
// passes the ledger is closed: pointLedgerOpen() answers false,
// decideBuyListing refuses every buy as a database error before any point
// statement, and a point statement called anyway throws a DatabaseError.
// The rest of the Exchange (listing, browsing, claims) does not need the
// ledger and keeps working. So a deployment whose UI_DB_* block names
// another server, a schema DB_USER may not write, or no schema at all runs
// without Exchange purchases, and the gameserver says so at startup
// (ExchangeService::openPointLedger) rather than at the first buy.
//
// The transaction pair. A purchase (completeExchangePurchase in
// exchange/ExchangePurchase.h) runs its writes between beginTransaction and
// commit under an ExchangeTransaction guard, which calls rollback on every
// other way out, a thrown DatabaseError included. The three are START
// TRANSACTION, COMMIT and ROLLBACK on the game connection, so the claim,
// the order and both ledger rows commit together or not at all.
//  - The writes run in one order: the claim (markListingSold, whose UPDATE
//    takes the listing's row lock and matches only an ACTIVE row), the
//    order, then the buyer's debit and the seller's credit. A failure at
//    any of them, refused or thrown, rolls all of them back.
//  - Every purchase takes its listing lock before its ledger locks, so two
//    purchases wait on each other in one order; a cycle they still form is
//    InnoDB's to detect, and its deadlock error is a thrown failure like
//    any other.
//  - A ROLLBACK with nothing begun, or after its COMMIT went through, is a
//    no-op.
//
// Collisions. Two buyers of one listing -- two zone threads of one server,
// or two servers of one world -- meet at the claim: the second UPDATE waits
// for the first purchase's row lock, then matches no ACTIVE row and is
// refused as no longer available, before either reaches ExchangeOrder's
// UNIQUE ListingID or the ledger's UNIQUE IdempotencyKey. A ledger key can
// still collide when two listings' purchases carry one client key: the
// count inside adjustPoints sees a committed one and answers false, and an
// uncommitted one makes the INSERT wait and then fail with ER_DUP_ENTRY.
// Either way the purchase rolls back, finds the key in the ledger and
// refuses as a replay.
//
// Text arguments (accounts, players, item names, idempotency keys) are
// escaped with mysql_real_escape_string on the game connection, or by
// a manual quote/backslash pass when no connection is up; the datetime
// text expireAt is interpolated raw. CreatedAt, UpdatedAt, SoldAt,
// CancelledAt and DeliveredAt are the server process's local time,
// formatted here; a caller's createdAt/updatedAt/deliveredAt fields are
// ignored on insert.
//
// ExchangeListing's UNIQUE KEY (ItemClass, ItemID, ObjectID), and the
// fact that no statement deletes a listing row, mean createListing for an
// object that was ever listed -- whatever status its old row is in now --
// fails with ER_DUP_ENTRY (thrown as a DatabaseError). ExchangeOrder's
// UNIQUE ListingID does the same to a second order on one listing.
//
// The methods let a SQL failure leave as an exception and keep the bool
// results the Exchange service was written against: a false from the
// other listing and order writes and from the transaction pair is
// unreachable (the SQL failure throws first); only markListingSold and
// adjustPoints answer false on their own.
//
// ExchangeListing is declared in GCExchangeList.h (a wire struct the
// client mirrors); ExchangeOrder and the status and reason codes are
// declared here. The listing loads read SELECT * positionally, in the
// CREATE TABLE column order.
//
// Not enclosed: no other file in the tree names these four tables in SQL.

// Listing status constants
const uint8_t LISTING_STATUS_ACTIVE = 0;
const uint8_t LISTING_STATUS_SOLD = 1;
const uint8_t LISTING_STATUS_CANCELLED = 2;
const uint8_t LISTING_STATUS_EXPIRED = 3;

// Order status constants
const uint8_t ORDER_STATUS_PAID = 0;
const uint8_t ORDER_STATUS_DELIVERED = 1;
const uint8_t ORDER_STATUS_CANCELLED = 2;

// Point transaction reason codes
const uint8_t POINT_REASON_BUY = 0;
const uint8_t POINT_REASON_SALE = 1;
const uint8_t POINT_REASON_TAX = 2;
const uint8_t POINT_REASON_REFUND = 3;
const uint8_t POINT_REASON_ADJUST = 4;

// Forward declaration (defined in GCExchangeList.h)
struct ExchangeListing;

// One ExchangeOrder row in column order. The two ids come back through
// getString and strtoll, serverID through getInt, status through getBYTE,
// the three datetimes as text ("" for NULL).
struct ExchangeOrder {
    int64_t orderID;
    int64_t listingID;
    int16_t serverID;
    std::string buyerAccount;
    std::string buyerPlayer;
    int pricePoint;
    int taxAmount;
    uint8_t status;
    std::string createdAt;
    std::string deliveredAt;
    std::string cancelledAt;
};

class ExchangeRepository {
public:
    virtual ~ExchangeRepository() {}

    // --- listings (the game schema) ----------------------------------------
    // INSERT of every column but ListingID, then SELECT LAST_INSERT_ID() on
    // the same Statement; the new id, or 0 when the id read answers no row
    // (it always answers one).
    virtual int64_t createListing(const ExchangeListing& listing) = 0;
    // Status = 2, CancelledAt and UpdatedAt = now, for an ACTIVE row only.
    // True whether or not a row matched.
    virtual bool cancelListing(int64_t listingID) = 0;
    // Status = 3, UpdatedAt = now, for an ACTIVE row only. True either way.
    virtual bool expireListing(int64_t listingID) = 0;
    // Status = 1, the buyer columns, SoldAt and UpdatedAt = now, for an
    // ACTIVE row only. True when it marked the row, false when the listing
    // was not ACTIVE (or has no row). Inside a transaction the row stays
    // locked until the transaction ends, which is what makes this the
    // purchase's claim.
    virtual bool markListingSold(int64_t listingID, const std::string& buyerAccount,
                                 const std::string& buyerPlayer) = 0;
    // One server's listings in one status, newest CreatedAt first, the
    // 1-based page of pageSize rows: LIMIT pageSize OFFSET (page - 1) *
    // pageSize.
    virtual std::vector<ExchangeListing> getListings(int16_t serverID, uint8_t status, int page, int pageSize) = 0;
    // A new ExchangeListing the caller owns, NULL when the id has no row.
    // Every caller in ExchangeService leaks it.
    virtual ExchangeListing* getListing(int64_t listingID) = 0;
    // One seller account's listings in one status, newest first.
    virtual std::vector<ExchangeListing> getSellerListings(const std::string& sellerAccount, uint8_t status) = 0;
    // Up to 1000 ACTIVE listings whose ExpireAt is before the database's
    // NOW(), oldest ExpireAt first.
    virtual std::vector<ExchangeListing> getExpiredListings() = 0;

    // --- orders (the game schema) ------------------------------------------
    // INSERT of the eight non-id columns (CreatedAt = now), then SELECT
    // LAST_INSERT_ID() on the same Statement; the new id, or 0.
    virtual int64_t createOrder(const ExchangeOrder& order) = 0;
    // Status = 1, DeliveredAt = now, for a PAID row only. True either way.
    virtual bool markOrderDelivered(int64_t orderID) = 0;
    // One buyer's orders in one status, newest first.
    virtual std::vector<ExchangeOrder> getBuyerOrders(const std::string& buyerPlayer, uint8_t status) = 0;
    // The orders in one status whose listing's SellerPlayer is this player
    // (INNER JOIN on ListingID), newest first.
    virtual std::vector<ExchangeOrder> getSellerOrders(const std::string& sellerPlayer, uint8_t status) = 0;

    // --- the point ledger (the account schema, by name) --------------------
    // Each throws a DatabaseError while the ledger is closed (see the note
    // above).
    //
    // Up to four statements on the game connection, each on its own
    // Statement:
    // when a key is given, a count of the PointLedger rows carrying it
    // (false, nothing written, when one exists); the account's
    // AccountPoint.PointBalance, read FOR UPDATE so a concurrent adjustment
    // of the account waits for this transaction instead of writing over
    // its sum (0 when it has no row); false, nothing
    // written, when balance + delta is below 0; otherwise REPLACE INTO
    // AccountPoint with the new balance and UpdatedAt = now, then an
    // INSERT INTO PointLedger -- with the IdempotencyKey column when a key
    // is given, without it otherwise -- and balanceAfter is the new
    // balance. The caller cannot tell the two false results apart.
    virtual bool adjustPoints(const std::string& account, int delta, int& balanceAfter, uint8_t reason,
                              int64_t refListingID, int64_t refOrderID, const std::string& idempotencyKey) = 0;
    // AccountPoint.PointBalance through getInt; 0 when the account has no
    // row.
    virtual int getPointBalance(const std::string& account) = 0;
    // Whether any PointLedger row carries this IdempotencyKey.
    virtual bool hasIdempotencyKey(const std::string& idempotencyKey) = 0;
    // Whether the point statements can run: false until a check of the
    // account schema passed (openExchangePointLedger), and after one that
    // failed.
    virtual bool pointLedgerOpen() = 0;

    // --- the transaction pair (see the note above) ------------------------
    // START TRANSACTION on the game connection. True, or a thrown
    // DatabaseError.
    virtual bool beginTransaction() = 0;
    // COMMIT on it. True, or a thrown DatabaseError.
    virtual bool commit() = 0;
    // ROLLBACK on it. True, or a thrown DatabaseError.
    virtual bool rollback() = 0;
};

// The process-wide MySQL-backed instance, wired in
// MySQLExchangeRepository.cpp.
ExchangeRepository& defaultExchangeRepository();

// Name the schema the point tables live in and check it from the calling
// thread's game connection (the checks are in the note above). True opens
// the default repository's point ledger; false closes it and says in
// failure which check failed and what MySQL answered. It writes state every
// thread reads, so it runs during single-threaded startup, before any
// thread can buy; the gameserver's call is ExchangeService::openPointLedger.
bool openExchangePointLedger(const std::string& accountSchema, std::string& failure);

#endif // __EXCHANGE_REPOSITORY_H__
