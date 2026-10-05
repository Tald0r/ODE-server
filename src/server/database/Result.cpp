//////////////////////////////////////////////////////////////////////////////
// File Name 	: Result.cc
// Written By	: Gday29@ewestsoft.com
// Description	: Result class implementation
//////////////////////////////////////////////////////////////////////////////

#include "Result.h"

#include "Assert.h"
#include "StringStream.h"

//////////////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////////////

Result::Result(T_RESULT* pResult, const string& statement) {
    __BEGIN_TRY

    m_pResult = pResult;
    m_pRow = NULL;

    Assert(m_pResult != NULL);

    // Set the #rows value.
    m_RowCount = mysql_num_rows(m_pResult);
    m_FieldCount = mysql_num_fields(m_pResult);

    m_Statement = statement;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////////////

Result::~Result() {
    __BEGIN_TRY

    if (m_pResult != NULL) {
        // If the result set is empty, don't call mysql_free_result
        // otherwise when you call malloc later you might run into
        // some nasty debugging issues.
        // [Note: man malloc has some interesting information to track
        // the problem like system varibles such as MALLOC_CHCK_]

        mysql_free_result(m_pResult);
        m_pResult = NULL;
    }

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// next()
// Returns true if a next row exists, false otherwise.
//////////////////////////////////////////////////////////////////////////////

bool Result::next() {
    __BEGIN_TRY

    Assert(m_pResult != NULL);

    return (m_pRow = mysql_fetch_row(m_pResult)) != NULL;

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
// getField
// Returns the column named by the index parameter.
// 1 is the first column (not 0).
//////////////////////////////////////////////////////////////////////////////

char* Result::getField(uint index) {
    __BEGIN_TRY

    if (m_pRow == NULL) {
        StringStream msg;
        msg << "Result::getField() : no current row; call Result::next() (field=" << index
            << ", field_count=" << m_FieldCount << ")";
        diagnosticFilelog("ResultBug.log", "%s", msg.toString().c_str());
        throw Error(msg.toString());
    }

    if (index == 0 || index > m_FieldCount) {
        StringStream msg;
        msg << "Result::getField() : field out of bounds (field=" << index << ", field_count=" << m_FieldCount << ")";
        diagnosticFilelog("ResultBug.log", "%s", msg.toString().c_str());
        throw OutOfBoundException(msg.toString());
    }

    return m_pRow[index - 1];

    __END_CATCH
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

const char* Result::getString(uint index) {
    __BEGIN_TRY

    static const char* pNULL = "";
    const char* pField = getField(index);

    if (pField == NULL) {
        return pNULL;
    }

    return pField;


    __END_CATCH
}
