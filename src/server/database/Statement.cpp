//////////////////////////////////////////////////////////////////////////////
// File Name 	: Statement.cpp
// Written by	: Gday29@ewestsoft.com
// Description	: SQL statement wrapper
//////////////////////////////////////////////////////////////////////////////

#include "Statement.h"

#include <stdarg.h>
#include <stdio.h>

#include <mysql/mysql.h>
#include <sys/time.h>

#include "Assert.h"
#include "Mutex.h"
#include "Profile.h"
#include "Result.h"

#define __FULL_PROFILE__

#ifndef __FULL_PROFILE__
#undef beginProfileEx
#define beginProfileEx(name) ((void)0)
#undef endProfileEx
#define endProfileEx(name) ((void)0)
#endif

namespace {

// Driver text can contain SQL literals, including credentials and private rows.
// Keep the operation and numeric driver code without copying that text.
std::string queryFailure(MYSQL* connection, const char* operation) {
    const auto code = mysql_errno(connection);
    const std::string message =
        std::string("Statement::executeQuery ") + operation + " failed (mysql_errno=" + std::to_string(code) + ")";
    try {
        std::ostream diagnostics(std::cerr.rdbuf());
        diagnostics << message << std::endl;
    } catch (...) {
    }
    return message;
}

// Format a printf-style statement into out. Answers false, leaving out
// untouched, when the formatted text is longer than
// Statement::kMaxStatementLength characters or the format fails: a statement
// that does not fit is refused whole, never truncated and executed.
bool formatStatement(string& out, const char* fmt, va_list valist) {
    char buffer[Statement::kMaxStatementLength + 1];

    const int nchars = vsnprintf(buffer, sizeof(buffer), fmt, valist);
    if (nchars < 0 || (size_t)nchars > Statement::kMaxStatementLength)
        return false;

    out.assign(buffer, (size_t)nchars);
    return true;
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////
Statement::Statement() {
    __BEGIN_TRY

    m_pConnection = NULL;
    m_pResult = NULL;
    m_nAffectedRows = 0;

    __END_CATCH
}

Statement::Statement(const char* fmt, ...) : m_pConnection(NULL), m_pResult(NULL), m_nAffectedRows(0) {
    __BEGIN_TRY

    va_list valist;
    va_start(valist, fmt);
    const bool fits = formatStatement(m_Statement, fmt, valist);
    va_end(valist);

    if (!fits)
        throw Error("more buffer size needed for SQL statement buffer...");

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// destructor
//
// Frees the result it created internally.
//
//////////////////////////////////////////////////////////////////////

Statement::~Statement() {
    if (m_pResult != NULL) {
        delete m_pResult;
        m_pResult = NULL;
    }
}


//////////////////////////////////////////////////////////////////////
//
// executeQuery()
//
// Takes the Connection and the SQL statement string and returns the
// resulting Result *.
//
//////////////////////////////////////////////////////////////////////

Result* Statement::executeQuery()

{
    __BEGIN_TRY

    Assert(m_pConnection != NULL);

    if (m_pResult != NULL) {
        // The application did not take the previous Result; drop it here.
        delete m_pResult;
        m_pResult = NULL;
    }

    beginProfileEx("ZPM_QUERY");

    if (mysql_real_query(m_pConnection->getMYSQL(), m_Statement.c_str(), m_Statement.size()) != 0) {
        throw SQLQueryException(queryFailure(m_pConnection->getMYSQL(), "query"));
    }

    MYSQL_RES* pResult = mysql_store_result(m_pConnection->getMYSQL());

    // A NULL result means the statement was an update, or an error.
    if (pResult != NULL) {
        m_pResult = new Result(pResult, m_Statement);
    } else {
        if (mysql_field_count(m_pConnection->getMYSQL()) != 0) {
            throw SQLQueryException(queryFailure(m_pConnection->getMYSQL(), "store result"));
        } else {
            m_nAffectedRows = mysql_affected_rows(m_pConnection->getMYSQL());
        }
    }

    endProfileEx("ZPM_QUERY");

    return m_pResult;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// executeQuery ()
//
// Takes the Connection and the SQL statement string and returns the
// resulting Result *.
//
//////////////////////////////////////////////////////////////////////

Result* Statement::executeQueryString(const string& sqlStatement) {
    __BEGIN_TRY

    m_Statement = sqlStatement;

    return executeQuery();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// executeQuery ()
//
// Takes the Connection and the SQL statement string and returns the
// resulting Result *.
//
//////////////////////////////////////////////////////////////////////

Result* Statement::executeQuery(const char* fmt, ...) {
    __BEGIN_TRY

    // Build the SQL statement.
    va_list valist;
    va_start(valist, fmt);
    const bool fits = formatStatement(m_Statement, fmt, valist);
    va_end(valist);

    if (!fits)
        throw Error("more buffer size needed for SQL statement buffer...");

    return executeQuery();

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
//	setStatement()
//
//	Rebuild the statement.
//
//////////////////////////////////////////////////////////////////////

void Statement::setStatement(const char* fmt, ...)

{
    __BEGIN_TRY

    va_list valist;
    va_start(valist, fmt);
    const bool fits = formatStatement(m_Statement, fmt, valist);
    va_end(valist);

    if (!fits)
        throw Error("more buffer size needed for SQL statement buffer...");

    __END_CATCH
}

uint Statement::getInsertID() const {
    __BEGIN_TRY
    return mysql_insert_id(m_pConnection->getMYSQL());
    __END_CATCH
}
