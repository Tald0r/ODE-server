#include "Assert.h"
#include "DB.h"
#include "ServerContext.h"
#include "repository/SMSMessageRepository.h"

// END_DB, except that the SQLQueryException is rethrown as itself instead
// of as DB.h's DatabaseError. The relay
// loop catches SQLQueryException to reopen the connection, and a
// DatabaseError reaches neither that branch nor its catch (Throwable&).
// DBError.log gets the same line END_DB writes, and the catch-all closes
// the statement for every other exception the block can raise.
#define END_DB_RETHROW(STMT)                                            \
    catch (SQLQueryException & sqe) {                                   \
        delete STMT;                                                    \
        string msg;                                                     \
        msg += string(std::source_location::current().function_name()); \
        msg += " : ";                                                   \
        msg += string(sqe.toString());                                  \
        diagnosticFilelog("DBError.log", "%s", msg.c_str());            \
        throw;                                                          \
    }                                                                   \
    catch (...) {                                                       \
        delete STMT;                                                    \
        throw;                                                          \
    }

namespace {

// MySQL implementation of SMSMessageRepository. Quirks:
//  - The connection is this repository's own, held in a member rather
//    than taken per call from DatabaseManager, because the relay is a
//    different server from the game's.
//  - reopen() names no port, so the client library's default is used and
//    a relay on any other port is unreachable after a reconnect. It also
//    deletes the old connection before opening the new one, so a failing
//    open leaves the repository holding nothing and the next statement
//    dereferences NULL.
//  - The id predicate builds its LIKE pattern from three %c digits and a
//    literal %% wildcard, and compares length(mid) to the id width, so a
//    row of another width sharing the prefix is not counted.
//  - target, toname and callback are interpolated raw; body is escaped
//    by the caller (quotes and backslashes doubled) before it arrives.
//  - The queue row is a second statement rather than the same one the
//    message row used; either can fail on its own, leaving a message
//    inserted but never queued.
class MySQLSMSMessageRepository : public SMSMessageRepository {
public:
    MySQLSMSMessageRepository() : m_pConnection(NULL) {}

    void open(const string& host, const string& db, const string& user, const string& password, uint port) {
        m_pConnection = new Connection(host, db, user, password, port);
        Assert(m_pConnection != NULL);
    }

    void reopen(const string& host, const string& db, const string& user, const string& password) {
        SAFE_DELETE(m_pConnection);

        m_pConnection = new Connection(host, db, user, password);
        Assert(m_pConnection != NULL);
    }

    bool loadMaxMessageID(int dimensionDigit, int worldDigit, int serverDigit, int keySize, string& maxID) {
        bool found = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = m_pConnection->createStatement();
            Result* pResult =
                pStmt->executeQuery("SELECT MAX(mid) FROM uds_msg WHERE mid LIKE '%c%c%c%%' AND length(mid)=%d",
                                    dimensionDigit, worldDigit, serverDigit, keySize);

            if (pResult->next()) {
                maxID = pResult->getString(1);
                found = true;
            }

            SAFE_DELETE(pStmt);
        }
        END_DB_RETHROW(pStmt)

        return found;
    }

    bool insertMessage(const string& mid, const string& target, const string& toName, const string& callback,
                       const string& body) {
        bool inserted = false;
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = m_pConnection->createStatement();
            pStmt->executeQuery("INSERT INTO uds_msg (mid,recvdate,target,toname,callback,body) VALUES "
                                "('%s',now(),'%s','%s','%s','%s')",
                                mid.c_str(), target.c_str(), toName.c_str(), callback.c_str(), body.c_str());

            inserted = pStmt->getAffectedRowCount() != 0;

            SAFE_DELETE(pStmt);
        }
        END_DB_RETHROW(pStmt)

        return inserted;
    }

    void enqueue(const string& mid) {
        Statement* pStmt = NULL;

        BEGIN_DB {
            pStmt = m_pConnection->createStatement();
            pStmt->executeQuery("INSERT INTO msg_queue (mid) VALUES ('%s')", mid.c_str());

            SAFE_DELETE(pStmt);
        }
        END_DB_RETHROW(pStmt)
    }

    void keepAlive() {
        de::serverContext().database().executeDummyQuery(m_pConnection);
    }

private:
    Connection* m_pConnection;
};

} // namespace

SMSMessageRepository& defaultSMSMessageRepository() {
    static MySQLSMSMessageRepository instance;
    return instance;
}
