#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <vector>

#include <mysql/mysql.h> // For mysql_real_escape_string() in escapeSQL()

#include "DB.h"
#include "DatabaseManager.h"
#include "GCExchangeList.h" // For ExchangeListing definition
#include "ServerContext.h"
#include "StringStream.h"
#include "repository/ExchangeRepository.h"

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

namespace {

// The thread's game connection: every statement here runs on it, the point
// statements included, which name their tables by the account schema.
Connection* gameConnection() {
    return de::serverContext().database().getConnection("DARKEDEN");
}

// Get current timestamp in MySQL format
string getCurrentTime() {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    return string(buffer);
}

// Escape a value so it can be interpolated into a SINGLE-QUOTED SQL literal.
//
// What it guarantees:
//   * NUL bytes are dropped before anything else. Every caller in this file
//     builds its query with printf-style "%s" plus c_str(), which stops at the
//     first NUL - an embedded NUL would silently truncate the statement into
//     something malformed (or, worse, still valid but different).
//   * When the thread's database connection is reachable, the escaping is done
//     by mysql_real_escape_string(). That is the correct answer: it is aware of
//     the connection's character set (so a multi-byte lead byte is never
//     mistaken for an ASCII quote or backslash) and it follows the server's
//     NO_BACKSLASH_ESCAPES setting.
//   * Otherwise a manual escape runs, which handles BOTH the backslash and the
//     single quote. MySQL treats the backslash as an escape character inside
//     string literals by default, so doubling the quote alone is not enough: a
//     value such as  \' OR 1=1#  escapes its own doubled quote and breaks out
//     of the literal. It becomes  \\'' OR 1=1#  here, which the server reads
//     back as one literal backslash, one literal quote, and no statement break.
//     The pass maps each input byte independently in a single sweep, so an
//     escape it emits is never fed back through the loop. That is what makes it
//     correct without an ordering argument: a two-pass version would be safe
//     only because the quote is escaped by DOUBLING it (which introduces no new
//     backslashes for a backslash pass to re-double). Escape the quote as \'
//     instead and the order becomes load-bearing - quote pass first, backslash
//     pass second, and the \' turns into \\' with the quote left live. Do not
//     switch this to backslash-style quote escaping.
//
// What it does NOT guarantee:
//   * It is not a bound parameter. The result is only safe inside a
//     single-quoted string literal - never paste it into an identifier, a
//     double-quoted literal, an unquoted numeric slot, or a LIKE pattern
//     (% and _ stay live wildcards there).
//   * The manual fallback assumes an ASCII-compatible client character set and
//     the default backslash-escaping SQL mode. It is conservative and never
//     under-escapes, but under NO_BACKSLASH_ESCAPES it would double a backslash
//     the server takes literally, i.e. it can corrupt data where the library
//     path would not. Keep a connection available.
//   * Nothing about length. Callers remain responsible for column widths and
//     for the fixed statement buffer in Statement::executeQuery().
string escapeSQL(const string& input) {
    // Strip NUL bytes up front so the result can never truncate a "%s" query.
    string value;
    value.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] != '\0')
            value += input[i];
    }

    // Preferred path: let the client library escape against the live
    // connection. gameConnection() is a cheap per-thread lookup with no side
    // effects, and it hands back the very connection these queries run on.
    Connection* pConnection = gameConnection();
    if (pConnection != NULL && pConnection->isConnected()) {
        // mysql_real_escape_string() needs room for 2*length + 1 bytes.
        vector<char> buffer(value.size() * 2 + 1);
        unsigned long escapedLength =
            mysql_real_escape_string(pConnection->getMYSQL(), &buffer[0], value.c_str(), (unsigned long)value.size());
        // It reports (unsigned long)-1 only when NO_BACKSLASH_ESCAPES is active
        // and it cannot know the quoting character. Falling through to the
        // manual pass is then SAFE but not ideal: doubling the quote is exactly
        // right in that mode, while doubling the backslash - which the manual
        // pass also does - stores a backslash the server would have taken
        // literally. It never under-escapes, so it cannot open an injection;
        // it can only alter a value containing a backslash. This project
        // mandates a sql_mode without NO_BACKSLASH_ESCAPES (see CLAUDE.md), so
        // the path is unreachable in a correct deployment.
        if (escapedLength != (unsigned long)-1)
            return string(&buffer[0], escapedLength);
    }

    // Fallback: escape the backslash and the quote in a single pass.
    string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '\\')
            result += "\\\\";
        else if (c == '\'')
            result += "''";
        else
            result += c;
    }
    return result;
}

// Convert string to int64_t
int64_t toInt64(const string& str) {
    return strtoll(str.c_str(), NULL, 10);
}

// MySQL implementation of ExchangeRepository (see the header for the
// connection, the transaction pair and the escaping).
class MySQLExchangeRepository : public ExchangeRepository {
public:
    int64_t createListing(const ExchangeListing& listing);
    bool cancelListing(int64_t listingID);
    bool expireListing(int64_t listingID);
    bool markListingSold(int64_t listingID, const string& buyerAccount, const string& buyerPlayer);
    vector<ExchangeListing> getListings(int16_t serverID, uint8_t status, int page, int pageSize);
    ExchangeListing* getListing(int64_t listingID);
    vector<ExchangeListing> getSellerListings(const string& sellerAccount, uint8_t status);
    vector<ExchangeListing> getExpiredListings();

    int64_t createOrder(const ExchangeOrder& order);
    bool markOrderDelivered(int64_t orderID);
    vector<ExchangeOrder> getBuyerOrders(const string& buyerPlayer, uint8_t status);
    vector<ExchangeOrder> getSellerOrders(const string& sellerPlayer, uint8_t status);

    bool adjustPoints(const string& account, int delta, int& balanceAfter, uint8_t reason, int64_t refListingID,
                      int64_t refOrderID, const string& idempotencyKey);
    int getPointBalance(const string& account);
    bool hasIdempotencyKey(const string& idempotencyKey);
    bool pointLedgerOpen();

    bool beginTransaction();
    bool commit();
    bool rollback();

    // openExchangePointLedger's work (see the header).
    bool openPointLedger(const string& accountSchema, string& failure);

private:
    // The two point tables' names, qualified by the account schema.
    string accountPointTable() const;
    string pointLedgerTable() const;

    // The account schema as a quoted identifier followed by its dot, empty
    // while the ledger is closed. Written by openPointLedger during
    // single-threaded startup, read by every thread that buys afterwards.
    string m_PointSchema;
};

//////////////////////////////////////////////////////////////////////////////
// Listing operations
//////////////////////////////////////////////////////////////////////////////

// Create a new listing record
int64_t MySQLExchangeRepository::createListing(const ExchangeListing& listing) {
    Statement* pStmt = NULL;
    Result* pResult = NULL;

    BEGIN_DB {
        pStmt = gameConnection()->createStatement();

        // Build INSERT query - using printf-style format
        pStmt->executeQuery("INSERT INTO ExchangeListing ("
                            "ServerID, SellerAccount, SellerPlayer, SellerRace, "
                            "ItemClass, ItemType, ItemID, ObjectID, "
                            "PricePoint, Currency, Status, "
                            "TaxRate, TaxAmount, "
                            "CreatedAt, ExpireAt, UpdatedAt, Version, "
                            "ItemName, EnchantLevel, Grade, Durability, Silver, "
                            "OptionType1, OptionType2, OptionType3, "
                            "OptionValue1, OptionValue2, OptionValue3, StackCount"
                            ") VALUES (%d, '%s', '%s', %d, %d, %d, %lld, %d, %d, %d, %d, %d, %d, '%s', '%s', '%s', %d, "
                            "'%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
                            listing.serverID, escapeSQL(listing.sellerAccount).c_str(),
                            escapeSQL(listing.sellerPlayer).c_str(), (int)listing.sellerRace, (int)listing.itemClass,
                            listing.itemType, (long long)listing.itemID, listing.objectID, listing.pricePoint,
                            (int)listing.currency, (int)listing.status, (int)listing.taxRate, listing.taxAmount,
                            getCurrentTime().c_str(), listing.expireAt.c_str(), getCurrentTime().c_str(),
                            listing.version, escapeSQL(listing.itemName).c_str(), (int)listing.enchantLevel,
                            listing.grade, listing.durability, listing.silver, (int)listing.optionType1,
                            (int)listing.optionType2, (int)listing.optionType3, listing.optionValue1,
                            listing.optionValue2, listing.optionValue3, listing.stackCount);

        // Get the auto-generated listing ID
        pResult = pStmt->executeQuery("SELECT LAST_INSERT_ID()");
        if (pResult->next()) {
            int64_t listingID = toInt64(pResult->getString(1));
            SAFE_DELETE(pStmt);
            return listingID;
        }

        SAFE_DELETE(pStmt);
    }
    END_DB(pStmt)

    return 0;
}

// Cancel a listing (mark as CANCELLED)
bool MySQLExchangeRepository::cancelListing(int64_t listingID) {
    {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pStmt->executeQuery("UPDATE ExchangeListing SET "
                                "Status = 2, " // CANCELLED
                                "CancelledAt = '%s', "
                                "UpdatedAt = '%s' "
                                "WHERE ListingID = %lld "
                                "AND Status = 0", // Only ACTIVE listings
                                getCurrentTime().c_str(), getCurrentTime().c_str(), (long long)listingID);

            SAFE_DELETE(pStmt);
            return true;
        }
        END_DB(pStmt)
    }

    return false;
}

// Mark listing as expired
bool MySQLExchangeRepository::expireListing(int64_t listingID) {
    {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pStmt->executeQuery("UPDATE ExchangeListing SET "
                                "Status = 3, " // EXPIRED
                                "UpdatedAt = '%s' "
                                "WHERE ListingID = %lld "
                                "AND Status = 0", // Only ACTIVE listings
                                getCurrentTime().c_str(), (long long)listingID);

            SAFE_DELETE(pStmt);
            return true;
        }
        END_DB(pStmt)
    }

    return false;
}

// Mark listing as sold and set buyer info
bool MySQLExchangeRepository::markListingSold(int64_t listingID, const string& buyerAccount,
                                              const string& buyerPlayer) {
    {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pStmt->executeQuery("UPDATE ExchangeListing SET "
                                "Status = 1, " // SOLD
                                "BuyerAccount = '%s', "
                                "BuyerPlayer = '%s', "
                                "SoldAt = '%s', "
                                "UpdatedAt = '%s' "
                                "WHERE ListingID = %lld "
                                "AND Status = 0", // Only ACTIVE listings
                                escapeSQL(buyerAccount).c_str(), escapeSQL(buyerPlayer).c_str(),
                                getCurrentTime().c_str(), getCurrentTime().c_str(), (long long)listingID);

            // The statement sets Status from 0 to 1, so a matched row is always
            // a changed one and the affected count is the match.
            const bool marked = pStmt->getAffectedRowCount() != 0;
            SAFE_DELETE(pStmt);
            return marked;
        }
        END_DB(pStmt)
    }

    return false;
}

// Query listings with pagination and filters
vector<ExchangeListing> MySQLExchangeRepository::getListings(int16_t serverID, uint8_t status, int page, int pageSize) {
    vector<ExchangeListing> listings;

    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            int offset = (page - 1) * pageSize;

            pResult = pStmt->executeQuery("SELECT * FROM ExchangeListing "
                                          "WHERE ServerID = %d "
                                          "AND Status = %d "
                                          "ORDER BY CreatedAt DESC "
                                          "LIMIT %d OFFSET %d",
                                          serverID, (int)status, pageSize, offset);

            while (pResult->next()) {
                ExchangeListing listing;
                int idx = 1;

                listing.listingID = toInt64(pResult->getString(idx++));
                listing.serverID = pResult->getInt(idx++);
                listing.sellerAccount = pResult->getString(idx++);
                listing.sellerPlayer = pResult->getString(idx++);
                listing.sellerRace = pResult->getBYTE(idx++);
                listing.itemClass = pResult->getBYTE(idx++);
                listing.itemType = pResult->getWORD(idx++);
                listing.itemID = toInt64(pResult->getString(idx++));
                listing.objectID = pResult->getInt(idx++);
                listing.pricePoint = pResult->getInt(idx++);
                listing.currency = pResult->getBYTE(idx++);
                listing.status = pResult->getBYTE(idx++);
                listing.buyerAccount = pResult->getString(idx++);
                listing.buyerPlayer = pResult->getString(idx++);
                listing.taxRate = pResult->getBYTE(idx++);
                listing.taxAmount = pResult->getInt(idx++);
                listing.createdAt = pResult->getString(idx++);
                listing.expireAt = pResult->getString(idx++);
                listing.soldAt = pResult->getString(idx++);
                listing.cancelledAt = pResult->getString(idx++);
                listing.updatedAt = pResult->getString(idx++);
                listing.version = pResult->getInt(idx++);

                // Snapshot fields
                listing.itemName = pResult->getString(idx++);
                listing.enchantLevel = pResult->getBYTE(idx++);
                listing.grade = pResult->getWORD(idx++);
                listing.durability = pResult->getInt(idx++);
                listing.silver = pResult->getWORD(idx++);
                listing.optionType1 = pResult->getBYTE(idx++);
                listing.optionType2 = pResult->getBYTE(idx++);
                listing.optionType3 = pResult->getBYTE(idx++);
                listing.optionValue1 = pResult->getWORD(idx++);
                listing.optionValue2 = pResult->getWORD(idx++);
                listing.optionValue3 = pResult->getWORD(idx++);
                listing.stackCount = pResult->getInt(idx++);

                listings.push_back(listing);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return listings;
}

// Get a specific listing by ID
ExchangeListing* MySQLExchangeRepository::getListing(int64_t listingID) {
    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT * FROM ExchangeListing "
                                          "WHERE ListingID = %lld",
                                          (long long)listingID);

            if (pResult->next()) {
                ExchangeListing* listing = new ExchangeListing();
                int idx = 1;

                listing->listingID = toInt64(pResult->getString(idx++));
                listing->serverID = pResult->getInt(idx++);
                listing->sellerAccount = pResult->getString(idx++);
                listing->sellerPlayer = pResult->getString(idx++);
                listing->sellerRace = pResult->getBYTE(idx++);
                listing->itemClass = pResult->getBYTE(idx++);
                listing->itemType = pResult->getWORD(idx++);
                listing->itemID = toInt64(pResult->getString(idx++));
                listing->objectID = pResult->getInt(idx++);
                listing->pricePoint = pResult->getInt(idx++);
                listing->currency = pResult->getBYTE(idx++);
                listing->status = pResult->getBYTE(idx++);
                listing->buyerAccount = pResult->getString(idx++);
                listing->buyerPlayer = pResult->getString(idx++);
                listing->taxRate = pResult->getBYTE(idx++);
                listing->taxAmount = pResult->getInt(idx++);
                listing->createdAt = pResult->getString(idx++);
                listing->expireAt = pResult->getString(idx++);
                listing->soldAt = pResult->getString(idx++);
                listing->cancelledAt = pResult->getString(idx++);
                listing->updatedAt = pResult->getString(idx++);
                listing->version = pResult->getInt(idx++);

                // Snapshot fields
                listing->itemName = pResult->getString(idx++);
                listing->enchantLevel = pResult->getBYTE(idx++);
                listing->grade = pResult->getWORD(idx++);
                listing->durability = pResult->getInt(idx++);
                listing->silver = pResult->getWORD(idx++);
                listing->optionType1 = pResult->getBYTE(idx++);
                listing->optionType2 = pResult->getBYTE(idx++);
                listing->optionType3 = pResult->getBYTE(idx++);
                listing->optionValue1 = pResult->getWORD(idx++);
                listing->optionValue2 = pResult->getWORD(idx++);
                listing->optionValue3 = pResult->getWORD(idx++);
                listing->stackCount = pResult->getInt(idx++);

                SAFE_DELETE(pStmt);
                return listing;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return NULL;
}

// Get seller's listings
vector<ExchangeListing> MySQLExchangeRepository::getSellerListings(const string& sellerAccount, uint8_t status) {
    vector<ExchangeListing> listings;

    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT * FROM ExchangeListing "
                                          "WHERE SellerAccount = '%s' "
                                          "AND Status = %d "
                                          "ORDER BY CreatedAt DESC",
                                          escapeSQL(sellerAccount).c_str(), (int)status);

            while (pResult->next()) {
                ExchangeListing listing;
                int idx = 1;

                listing.listingID = toInt64(pResult->getString(idx++));
                listing.serverID = pResult->getInt(idx++);
                listing.sellerAccount = pResult->getString(idx++);
                listing.sellerPlayer = pResult->getString(idx++);
                listing.sellerRace = pResult->getBYTE(idx++);
                listing.itemClass = pResult->getBYTE(idx++);
                listing.itemType = pResult->getWORD(idx++);
                listing.itemID = toInt64(pResult->getString(idx++));
                listing.objectID = pResult->getInt(idx++);
                listing.pricePoint = pResult->getInt(idx++);
                listing.currency = pResult->getBYTE(idx++);
                listing.status = pResult->getBYTE(idx++);
                listing.buyerAccount = pResult->getString(idx++);
                listing.buyerPlayer = pResult->getString(idx++);
                listing.taxRate = pResult->getBYTE(idx++);
                listing.taxAmount = pResult->getInt(idx++);
                listing.createdAt = pResult->getString(idx++);
                listing.expireAt = pResult->getString(idx++);
                listing.soldAt = pResult->getString(idx++);
                listing.cancelledAt = pResult->getString(idx++);
                listing.updatedAt = pResult->getString(idx++);
                listing.version = pResult->getInt(idx++);

                // Snapshot fields
                listing.itemName = pResult->getString(idx++);
                listing.enchantLevel = pResult->getBYTE(idx++);
                listing.grade = pResult->getWORD(idx++);
                listing.durability = pResult->getInt(idx++);
                listing.silver = pResult->getWORD(idx++);
                listing.optionType1 = pResult->getBYTE(idx++);
                listing.optionType2 = pResult->getBYTE(idx++);
                listing.optionType3 = pResult->getBYTE(idx++);
                listing.optionValue1 = pResult->getWORD(idx++);
                listing.optionValue2 = pResult->getWORD(idx++);
                listing.optionValue3 = pResult->getWORD(idx++);
                listing.stackCount = pResult->getInt(idx++);

                listings.push_back(listing);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return listings;
}

// Get expired listings for maintenance scan
vector<ExchangeListing> MySQLExchangeRepository::getExpiredListings() {
    vector<ExchangeListing> listings;

    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            // Query active listings that have expired
            // Use NOW() to compare with ExpireAt
            pResult = pStmt->executeQuery("SELECT * FROM ExchangeListing "
                                          "WHERE Status = %d " // LISTING_STATUS_ACTIVE = 0
                                          "AND ExpireAt < NOW() "
                                          "ORDER BY ExpireAt ASC "
                                          "LIMIT 1000", // Process in batches to avoid long transactions
                                          (int)LISTING_STATUS_ACTIVE);

            while (pResult->next()) {
                ExchangeListing listing;
                int idx = 1;

                listing.listingID = toInt64(pResult->getString(idx++));
                listing.serverID = pResult->getInt(idx++);
                listing.sellerAccount = pResult->getString(idx++);
                listing.sellerPlayer = pResult->getString(idx++);
                listing.sellerRace = pResult->getBYTE(idx++);
                listing.itemClass = pResult->getBYTE(idx++);
                listing.itemType = pResult->getWORD(idx++);
                listing.itemID = toInt64(pResult->getString(idx++));
                listing.objectID = pResult->getInt(idx++);
                listing.pricePoint = pResult->getInt(idx++);
                listing.currency = pResult->getBYTE(idx++);
                listing.status = pResult->getBYTE(idx++);
                listing.buyerAccount = pResult->getString(idx++);
                listing.buyerPlayer = pResult->getString(idx++);
                listing.taxRate = pResult->getBYTE(idx++);
                listing.taxAmount = pResult->getInt(idx++);
                listing.createdAt = pResult->getString(idx++);
                listing.expireAt = pResult->getString(idx++);
                listing.soldAt = pResult->getString(idx++);
                listing.cancelledAt = pResult->getString(idx++);
                listing.updatedAt = pResult->getString(idx++);
                listing.version = pResult->getInt(idx++);

                // Snapshot fields
                listing.itemName = pResult->getString(idx++);
                listing.enchantLevel = pResult->getBYTE(idx++);
                listing.grade = pResult->getWORD(idx++);
                listing.durability = pResult->getInt(idx++);
                listing.silver = pResult->getWORD(idx++);
                listing.optionType1 = pResult->getBYTE(idx++);
                listing.optionType2 = pResult->getBYTE(idx++);
                listing.optionType3 = pResult->getBYTE(idx++);
                listing.optionValue1 = pResult->getWORD(idx++);
                listing.optionValue2 = pResult->getWORD(idx++);
                listing.optionValue3 = pResult->getWORD(idx++);
                listing.stackCount = pResult->getInt(idx++);

                listings.push_back(listing);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return listings;
}

//////////////////////////////////////////////////////////////////////////////
// Order operations
//////////////////////////////////////////////////////////////////////////////

// Create a new order
int64_t MySQLExchangeRepository::createOrder(const ExchangeOrder& order) {
    Statement* pStmt = NULL;
    Result* pResult = NULL;

    BEGIN_DB {
        pStmt = gameConnection()->createStatement();

        pStmt->executeQuery("INSERT INTO ExchangeOrder ("
                            "ListingID, ServerID, BuyerAccount, BuyerPlayer, "
                            "PricePoint, TaxAmount, Status, CreatedAt"
                            ") VALUES (%lld, %d, '%s', '%s', %d, %d, %d, '%s')",
                            (long long)order.listingID, order.serverID, escapeSQL(order.buyerAccount).c_str(),
                            escapeSQL(order.buyerPlayer).c_str(), order.pricePoint, order.taxAmount, (int)order.status,
                            getCurrentTime().c_str());

        // Get the auto-generated order ID
        pResult = pStmt->executeQuery("SELECT LAST_INSERT_ID()");
        if (pResult->next()) {
            int64_t orderID = toInt64(pResult->getString(1));
            SAFE_DELETE(pStmt);
            return orderID;
        }

        SAFE_DELETE(pStmt);
    }
    END_DB(pStmt)

    return 0;
}

// Mark order as delivered
bool MySQLExchangeRepository::markOrderDelivered(int64_t orderID) {
    {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pStmt->executeQuery("UPDATE ExchangeOrder SET "
                                "Status = 1, " // DELIVERED
                                "DeliveredAt = '%s' "
                                "WHERE OrderID = %lld "
                                "AND Status = 0", // Only PAID orders
                                getCurrentTime().c_str(), (long long)orderID);

            SAFE_DELETE(pStmt);
            return true;
        }
        END_DB(pStmt)
    }

    return false;
}

// Get buyer's orders
vector<ExchangeOrder> MySQLExchangeRepository::getBuyerOrders(const string& buyerPlayer, uint8_t status) {
    vector<ExchangeOrder> orders;

    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT * FROM ExchangeOrder "
                                          "WHERE BuyerPlayer = '%s' "
                                          "AND Status = %d "
                                          "ORDER BY CreatedAt DESC",
                                          escapeSQL(buyerPlayer).c_str(), (int)status);

            while (pResult->next()) {
                ExchangeOrder order;
                int idx = 1;

                order.orderID = toInt64(pResult->getString(idx++));
                order.listingID = toInt64(pResult->getString(idx++));
                order.serverID = pResult->getInt(idx++);
                order.buyerAccount = pResult->getString(idx++);
                order.buyerPlayer = pResult->getString(idx++);
                order.pricePoint = pResult->getInt(idx++);
                order.taxAmount = pResult->getInt(idx++);
                order.status = pResult->getBYTE(idx++);
                order.createdAt = pResult->getString(idx++);
                order.deliveredAt = pResult->getString(idx++);
                order.cancelledAt = pResult->getString(idx++);

                orders.push_back(order);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return orders;
}

// Get seller's fulfilled orders
vector<ExchangeOrder> MySQLExchangeRepository::getSellerOrders(const string& sellerPlayer, uint8_t status) {
    vector<ExchangeOrder> orders;

    {
        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT o.* FROM ExchangeOrder o "
                                          "INNER JOIN ExchangeListing l ON o.ListingID = l.ListingID "
                                          "WHERE l.SellerPlayer = '%s' "
                                          "AND o.Status = %d "
                                          "ORDER BY o.CreatedAt DESC",
                                          escapeSQL(sellerPlayer).c_str(), (int)status);

            while (pResult->next()) {
                ExchangeOrder order;
                int idx = 1;

                order.orderID = toInt64(pResult->getString(idx++));
                order.listingID = toInt64(pResult->getString(idx++));
                order.serverID = pResult->getInt(idx++);
                order.buyerAccount = pResult->getString(idx++);
                order.buyerPlayer = pResult->getString(idx++);
                order.pricePoint = pResult->getInt(idx++);
                order.taxAmount = pResult->getInt(idx++);
                order.status = pResult->getBYTE(idx++);
                order.createdAt = pResult->getString(idx++);
                order.deliveredAt = pResult->getString(idx++);
                order.cancelledAt = pResult->getString(idx++);

                orders.push_back(order);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return orders;
}

//////////////////////////////////////////////////////////////////////////////
// Point operations (the account schema's tables, on the game connection)
//////////////////////////////////////////////////////////////////////////////

string MySQLExchangeRepository::accountPointTable() const {
    // A closed ledger has no schema to name. The buy decision refuses before
    // any point statement while it is closed, so reaching this is a caller
    // that skipped that check; it fails like a statement would.
    if (m_PointSchema.empty())
        throw DatabaseError("the Exchange point ledger is not open (see openExchangePointLedger)");
    return m_PointSchema + "AccountPoint";
}

string MySQLExchangeRepository::pointLedgerTable() const {
    if (m_PointSchema.empty())
        throw DatabaseError("the Exchange point ledger is not open (see openExchangePointLedger)");
    return m_PointSchema + "PointLedger";
}

bool MySQLExchangeRepository::pointLedgerOpen() {
    return !m_PointSchema.empty();
}

// Adjust point balance with ledger record
bool MySQLExchangeRepository::adjustPoints(const string& account, int delta, int& balanceAfter, uint8_t reason,
                                           int64_t refListingID, int64_t refOrderID, const string& idempotencyKey) {
    {
        const string accountPoint = accountPointTable();
        const string pointLedger = pointLedgerTable();

        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            Connection* pConn = gameConnection();

            // Check idempotency if key provided
            if (!idempotencyKey.empty()) {
                pStmt = pConn->createStatement();
                pResult = pStmt->executeQuery("SELECT COUNT(*) FROM %s WHERE IdempotencyKey = '%s'",
                                              pointLedger.c_str(), escapeSQL(idempotencyKey).c_str());

                if (pResult->next() && pResult->getInt(1) > 0) {
                    // Idempotency key exists - already processed
                    SAFE_DELETE(pStmt);
                    return false;
                }
                SAFE_DELETE(pStmt);
            }

            // The current balance, read under the row's lock: the new balance is
            // written back as an absolute value, so a plain read would let two
            // concurrent adjustments of one account each write their own sum
            // and lose the other's. The lock holds until the transaction ends,
            // so the read and the write are one step only inside a transaction;
            // in autocommit mode it is released with this statement.
            pStmt = pConn->createStatement();
            pResult = pStmt->executeQuery("SELECT PointBalance FROM %s WHERE Account = '%s' FOR UPDATE",
                                          accountPoint.c_str(), escapeSQL(account).c_str());

            int currentBalance = 0;
            if (pResult->next()) {
                currentBalance = pResult->getInt(1);
            }
            SAFE_DELETE(pStmt);

            // Calculate new balance
            int newBalance = currentBalance + delta;
            if (newBalance < 0) {
                // Insufficient balance
                return false;
            }

            // Update or insert balance using REPLACE
            pStmt = pConn->createStatement();
            pStmt->executeQuery("REPLACE INTO %s (Account, PointBalance, UpdatedAt) "
                                "VALUES ('%s', %d, '%s')",
                                accountPoint.c_str(), escapeSQL(account).c_str(), newBalance, getCurrentTime().c_str());
            SAFE_DELETE(pStmt);

            // Insert ledger record
            pStmt = pConn->createStatement();
            if (idempotencyKey.empty()) {
                pStmt->executeQuery("INSERT INTO %s "
                                    "(Account, Delta, BalanceAfter, Reason, RefListingID, RefOrderID, CreatedAt) "
                                    "VALUES ('%s', %d, %d, %d, %lld, %lld, '%s')",
                                    pointLedger.c_str(), escapeSQL(account).c_str(), delta, newBalance, (int)reason,
                                    (long long)refListingID, (long long)refOrderID, getCurrentTime().c_str());
            } else {
                pStmt->executeQuery(
                    "INSERT INTO %s "
                    "(Account, Delta, BalanceAfter, Reason, RefListingID, RefOrderID, IdempotencyKey, CreatedAt) "
                    "VALUES ('%s', %d, %d, %d, %lld, %lld, '%s', '%s')",
                    pointLedger.c_str(), escapeSQL(account).c_str(), delta, newBalance, (int)reason,
                    (long long)refListingID, (long long)refOrderID, escapeSQL(idempotencyKey).c_str(),
                    getCurrentTime().c_str());
            }
            SAFE_DELETE(pStmt);

            balanceAfter = newBalance;
            return true;
        }
        END_DB(pStmt)
    }

    return false;
}

// Get current point balance
int MySQLExchangeRepository::getPointBalance(const string& account) {
    {
        const string accountPoint = accountPointTable();

        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT PointBalance FROM %s WHERE Account = '%s'", accountPoint.c_str(),
                                          escapeSQL(account).c_str());

            if (pResult->next()) {
                int balance = pResult->getInt(1);
                SAFE_DELETE(pStmt);
                return balance;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return 0;
}

// Check if idempotency key exists
bool MySQLExchangeRepository::hasIdempotencyKey(const string& idempotencyKey) {
    {
        const string pointLedger = pointLedgerTable();

        Statement* pStmt = NULL;
        Result* pResult = NULL;

        BEGIN_DB {
            pStmt = gameConnection()->createStatement();

            pResult = pStmt->executeQuery("SELECT COUNT(*) FROM %s WHERE IdempotencyKey = '%s'", pointLedger.c_str(),
                                          escapeSQL(idempotencyKey).c_str());

            if (pResult->next()) {
                int count = pResult->getInt(1);
                SAFE_DELETE(pStmt);
                return (count > 0);
            }

            SAFE_DELETE(pStmt);
        }
        END_DB(pStmt)
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Opening the point ledger
//////////////////////////////////////////////////////////////////////////////

// One statement of the opening checks: what it proves when it runs, and its
// text. A failure is reported with both and MySQL's own message.
struct LedgerCheck {
    const char* proves;
    string sql;
};

// Run one statement on one connection and keep the first value it answers.
// False, with the driver's message in failure, when the statement fails.
bool runLedgerCheck(Connection* pConnection, const string& sql, string& firstValue, string& failure) {
    Statement* pStmt = NULL;
    try {
        pStmt = pConnection->createStatement();
        Result* pResult = pStmt->executeQueryString(sql);
        if (pResult != NULL && pResult->next())
            firstValue = pResult->getString(1);
        SAFE_DELETE(pStmt);
        return true;
    } catch (SQLQueryException& sqe) {
        SAFE_DELETE(pStmt);
        failure = sqe.toString();
        return false;
    }
}

bool MySQLExchangeRepository::openPointLedger(const string& accountSchema, string& failure) {
    m_PointSchema.clear();

    // The name goes into the statements as a quoted identifier, so it may
    // hold anything but the quote itself.
    if (accountSchema.empty() || accountSchema.find('`') != string::npos || accountSchema.find('\0') != string::npos) {
        failure = "'" + accountSchema + "' is not a schema name the statements can quote";
        return false;
    }
    const string schema = "`" + accountSchema + "`.";

    Connection* pGame = gameConnection();
    if (pGame == NULL) {
        failure = "this thread has no game connection to run the point statements on";
        return false;
    }

    // One server: the account connection and the game connection must reach
    // the same MySQL server, or a schema of that name on the game server
    // would be a different set of balances from the one the account
    // database holds.
    Connection* pAccount = de::serverContext().database().getUserInfoConnection();
    if (pAccount == NULL) {
        failure = "there is no account connection to compare the game connection's server with";
        return false;
    }
    string gameServer;
    string accountServer;
    string checkFailure;
    if (!runLedgerCheck(pGame, "SELECT @@server_uuid", gameServer, checkFailure)) {
        failure = "SELECT @@server_uuid on the game connection failed: " + checkFailure;
        return false;
    }
    if (!runLedgerCheck(pAccount, "SELECT @@server_uuid", accountServer, checkFailure)) {
        failure = "SELECT @@server_uuid on the account connection failed: " + checkFailure;
        return false;
    }
    if (gameServer != accountServer) {
        failure = "the game connection reaches server " + gameServer + " and the account connection server " +
                  accountServer + " (SELECT @@server_uuid): they are not one server";
        return false;
    }

    // Every statement shape the ledger issues, run so that it touches no row:
    // each needs the table, the columns it names and the privilege the real
    // statement needs (SELECT, the row lock, INSERT, and REPLACE's INSERT and
    // DELETE), which MySQL checks before it looks for a row.
    const LedgerCheck checks[] = {
        {"the balance read and its row lock",
         "SELECT PointBalance FROM " + schema + "AccountPoint WHERE Account = '' AND FALSE FOR UPDATE"},
        {"the ledger key lookup", "SELECT COUNT(*) FROM " + schema + "PointLedger WHERE IdempotencyKey = '' AND FALSE"},
        {"the balance write", "REPLACE INTO " + schema +
                                  "AccountPoint (Account, PointBalance, UpdatedAt) "
                                  "SELECT '', 0, NOW() FROM DUAL WHERE FALSE"},
        {"the ledger row write", "INSERT INTO " + schema +
                                     "PointLedger (Account, Delta, BalanceAfter, Reason, RefListingID, RefOrderID, "
                                     "IdempotencyKey, CreatedAt) SELECT '', 0, 0, 0, 0, 0, '', NOW() FROM DUAL "
                                     "WHERE FALSE"},
    };
    for (const LedgerCheck& check : checks) {
        string ignored;
        if (!runLedgerCheck(pGame, check.sql, ignored, checkFailure)) {
            failure = string(check.proves) + " cannot run on the game connection (" + check.sql + "): " + checkFailure;
            return false;
        }
    }

    m_PointSchema = schema;
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// The transaction pair
//////////////////////////////////////////////////////////////////////////////

// One transaction-control statement on the thread's game connection.
void runTransactionStatement(const char* sql) {
    Statement* pStmt = NULL;

    BEGIN_DB {
        pStmt = gameConnection()->createStatement();
        pStmt->executeQueryString(sql);
        SAFE_DELETE(pStmt);
    }
    END_DB(pStmt)
}

bool MySQLExchangeRepository::beginTransaction() {
    runTransactionStatement("START TRANSACTION");
    return true;
}

bool MySQLExchangeRepository::commit() {
    runTransactionStatement("COMMIT");
    return true;
}

bool MySQLExchangeRepository::rollback() {
    runTransactionStatement("ROLLBACK");
    return true;
}

MySQLExchangeRepository& theRepository() {
    static MySQLExchangeRepository instance;
    return instance;
}

} // namespace

ExchangeRepository& defaultExchangeRepository() {
    return theRepository();
}

bool openExchangePointLedger(const std::string& accountSchema, std::string& failure) {
    return theRepository().openPointLedger(accountSchema, failure);
}
