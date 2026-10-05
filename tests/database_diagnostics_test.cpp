#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "Connection.h"
#include "Result.h"
#include "Statement.h"

namespace driver {
bool queryFails = false;
bool resultFails = false;
bool rowRead = false;
unsigned freed = 0;
unsigned errorsRead = 0;
std::string submitted;
MYSQL_RES result{};
char privateRow[] = "private-row-marker";
char* row[] = {privateRow};
} // namespace driver

// Replace only the external driver boundary. The production Statement and
// Result methods below execute unchanged; no database or connection is opened.
Connection::Connection() : m_Mysql{}, m_bConnected(false), m_Port(0), m_bBusy(false) {}
Connection::~Connection() = default;
extern "C" {
int STDCALL mysql_real_query(MYSQL*, const char* query, unsigned long length) {
    driver::submitted.assign(query, length);
    driver::rowRead = false;
    return driver::queryFails ? 1 : 0;
}
unsigned int STDCALL mysql_errno(MYSQL*) {
    return 1064;
}
const char* STDCALL mysql_error(MYSQL*) {
    ++driver::errorsRead;
    return "syntax error near private-driver-marker";
}
MYSQL_RES* STDCALL mysql_store_result(MYSQL*) {
    return driver::resultFails ? nullptr : &driver::result;
}
unsigned int STDCALL mysql_field_count(MYSQL*) {
    return 1;
}
my_ulonglong STDCALL mysql_affected_rows(MYSQL*) {
    return 0;
}
my_ulonglong STDCALL mysql_insert_id(MYSQL*) {
    return 0;
}
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES*) {
    return 1;
}
unsigned int STDCALL mysql_num_fields(MYSQL_RES*) {
    return 1;
}
MYSQL_ROW STDCALL mysql_fetch_row(MYSQL_RES*) {
    if (driver::rowRead)
        return nullptr;
    driver::rowRead = true;
    return driver::row;
}
void STDCALL mysql_free_result(MYSQL_RES*) {
    ++driver::freed;
}
}

namespace {
class DatabaseDiagnostics : public ::testing::Test {
protected:
    void SetUp() override {
        driver::queryFails = driver::resultFails = driver::rowRead = false;
        driver::freed = driver::errorsRead = 0;
        driver::submitted.clear();
        original = std::filesystem::current_path();
        std::string pattern = (std::filesystem::temp_directory_path() / "database-diagnostics-XXXXXX").string();
        ASSERT_NE(nullptr, ::mkdtemp(pattern.data()));
        directory = pattern;
        std::filesystem::current_path(directory);
        previousOut = std::cout.rdbuf(output.rdbuf());
        previousError = std::cerr.rdbuf(errors.rdbuf());
    }
    void TearDown() override {
        std::cout.rdbuf(previousOut);
        std::cerr.rdbuf(previousError);
        std::filesystem::current_path(original);
        std::filesystem::remove_all(directory);
    }
    void expectPrivate(const std::string& text) {
        for (const char* value : {"private-sql-marker", "private-driver-marker", "private-row-marker", "SELECT"})
            EXPECT_EQ(text.find(value), std::string::npos);
    }
    std::string resultLog() {
        std::ifstream input("ResultBug.log");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    std::filesystem::path original, directory;
    std::ostringstream output, errors;
    std::streambuf *previousOut = nullptr, *previousError = nullptr;
};

TEST_F(DatabaseDiagnostics, QueryFailureKeepsNumericCodeAndCanRetryWithoutLeakingSqlOrDriverText) {
    Connection connection;
    {
        Statement statement;
        statement.setConnection(&connection);
        driver::queryFails = true;
        try {
            statement.executeQueryString("SELECT 'private-sql-marker'");
            FAIL() << "query unexpectedly succeeded";
        } catch (const SQLQueryException& error) {
            expectPrivate(error.toString());
            EXPECT_NE(error.toString().find("query failed (mysql_errno=1064)"), std::string::npos);
            EXPECT_NE(error.toString().find("Statement::executeQuery"), std::string::npos);
        }
        EXPECT_EQ(driver::submitted, "SELECT 'private-sql-marker'");
        EXPECT_EQ(driver::freed, 0u);
        driver::queryFails = false;
        Result* result = statement.executeQuery();
        ASSERT_NE(result, nullptr);
        ASSERT_TRUE(result->next());
        EXPECT_STREQ(result->getString(1), "private-row-marker");
        EXPECT_EQ(driver::freed, 0u);
    }
    EXPECT_EQ(driver::freed, 1u);
    expectPrivate(output.str());
    expectPrivate(errors.str());
    EXPECT_EQ(driver::errorsRead, 0u);
}

TEST_F(DatabaseDiagnostics, StoreFailureReleasesPreviousResultAndRetainsItsOperationAndCode) {
    Connection connection;
    {
        Statement statement;
        statement.setConnection(&connection);
        ASSERT_NE(statement.executeQueryString("SELECT 'private-sql-marker'"), nullptr);
        driver::resultFails = true;
        try {
            statement.executeQuery();
            FAIL() << "result unexpectedly succeeded";
        } catch (const SQLQueryException& error) {
            expectPrivate(error.toString());
            EXPECT_NE(error.toString().find("store result failed (mysql_errno=1064)"), std::string::npos);
        }
        EXPECT_EQ(driver::freed, 1u);
        driver::resultFails = false;
        ASSERT_NE(statement.executeQuery(), nullptr);
    }
    EXPECT_EQ(driver::freed, 2u);
    expectPrivate(output.str());
    expectPrivate(errors.str());
    EXPECT_EQ(driver::errorsRead, 0u);
}

TEST_F(DatabaseDiagnostics, InvalidRowAccessKeepsExceptionTypesAndBoundsWithoutStatementOrRowData) {
    {
        Result result(&driver::result, "SELECT 'private-sql-marker'");
        try {
            result.getString(1);
            FAIL() << "missing row accepted";
        } catch (const Error& error) {
            expectPrivate(error.toString());
            EXPECT_NE(error.toString().find("Result::getField"), std::string::npos);
            EXPECT_NE(error.toString().find("no current row"), std::string::npos);
        }
        ASSERT_TRUE(result.next());
        for (unsigned index : {0u, 2u}) {
            try {
                result.getString(index);
                FAIL() << "invalid field accepted";
            } catch (const OutOfBoundException& error) {
                expectPrivate(error.toString());
                EXPECT_NE(error.toString().find("field=" + std::to_string(index)), std::string::npos);
                EXPECT_NE(error.toString().find("field_count=1"), std::string::npos);
            }
        }
        EXPECT_STREQ(result.getString(1), "private-row-marker");
    }
    EXPECT_EQ(driver::freed, 1u);
    expectPrivate(resultLog());
    expectPrivate(output.str());
    expectPrivate(errors.str());
}

class BrokenOutput : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override {
        throw std::runtime_error("broken output");
    }
};

TEST_F(DatabaseDiagnostics, FailedDiagnosticOutputDoesNotReplaceSqlFailureOrPoisonErrorStream) {
    Connection connection;
    Statement statement;
    statement.setConnection(&connection);
    driver::queryFails = true;
    BrokenOutput sink;
    auto* previous = std::cerr.rdbuf(&sink);
    const auto originalState = std::cerr.rdstate();
    EXPECT_THROW(statement.executeQueryString("SELECT 'private-sql-marker'"), SQLQueryException);
    EXPECT_EQ(std::cerr.rdstate(), originalState);
    std::cerr.rdbuf(previous);
}
} // namespace
