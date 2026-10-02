#include <ios>
#include <sstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Properties.h"
#include "PropertiesParser.h"

namespace {

TEST(PropertiesParser, ReadsTheFinalPropertyWithoutATrailingNewline) {
    std::istringstream input("first : one\nlast : two");
    Properties properties;
    de::readProperties(input, properties);
    EXPECT_EQ("one", properties.getProperty("first"));
    ASSERT_TRUE(properties.hasKey("last"));
    EXPECT_EQ("two", properties.getProperty("last"));
}

TEST(PropertiesParser, RejectsAFinalMalformedLineWithoutATrailingNewline) {
    std::istringstream input("first : one\nmissing separator");
    Properties properties;
    EXPECT_THROW(de::readProperties(input, properties), IOException);
    EXPECT_EQ("one", properties.getProperty("first"));
}

TEST(PropertiesParser, RejectsAFinalMissingValueWithoutATrailingNewline) {
    std::istringstream input("last : \t");
    Properties properties;
    EXPECT_THROW(de::readProperties(input, properties), IOException);
    EXPECT_FALSE(properties.hasKey("last"));
}

TEST(PropertiesParser, TrimsSpacesAndTabsAroundKeysAndValues) {
    std::istringstream input(" \tname with spaces \t: \t value with spaces \t\n");
    Properties properties;
    de::readProperties(input, properties);
    EXPECT_EQ("value with spaces", properties.getProperty("name with spaces"));
}

TEST(PropertiesParser, SkipsEmptyLinesWhitespaceAndColumnZeroComments) {
    std::istringstream input("\n \t\n#comment without a separator\nkey : value\n# final comment");
    Properties properties;
    de::readProperties(input, properties);
    EXPECT_EQ("key : value\n", properties.toString());
}

TEST(PropertiesParser, SeparatorsAndHashCharactersWithinValuesAreLiteral) {
    std::istringstream input("URL : http://host:1234/path#fragment\nPassword : #literal:secret\n");
    Properties properties;
    de::readProperties(input, properties);
    EXPECT_EQ("http://host:1234/path#fragment", properties.getProperty("URL"));
    EXPECT_EQ("#literal:secret", properties.getProperty("Password"));
}

TEST(PropertiesParser, MergesExistingPropertiesAndLetsTheLastDuplicateWin) {
    Properties properties;
    properties.setProperty("keep", "original");
    properties.setProperty("replace", "original");
    std::istringstream input("replace : first\nreplace : last\n");
    de::readProperties(input, properties);
    EXPECT_EQ("original", properties.getProperty("keep"));
    EXPECT_EQ("last", properties.getProperty("replace"));
}

TEST(PropertiesParser, EmptyInputLeavesExistingPropertiesAlone) {
    Properties properties;
    properties.setProperty("keep", "original");
    std::istringstream input;
    EXPECT_NO_THROW(de::readProperties(input, properties));
    EXPECT_EQ("keep : original\n", properties.toString());
}

struct InvalidLine {
    const char* text;
    const char* message;
};

class InvalidPropertiesTest : public ::testing::TestWithParam<InvalidLine> {};

TEST_P(InvalidPropertiesTest, ReportsTheParseErrorAndKeepsOnlyEarlierEntries) {
    for (const char* ending : {"\n", ""}) {
        SCOPED_TRACE(ending[0] ? "with newline" : "without newline");
        std::istringstream input(std::string("valid : earlier\n") + GetParam().text + ending);
        Properties properties;
        try {
            de::readProperties(input, properties);
            FAIL() << "malformed property accepted";
        } catch (const IOException& error) {
            EXPECT_NE(std::string::npos, error.toString().find(GetParam().message));
        }
        EXPECT_EQ("valid : earlier\n", properties.toString());
    }
}

INSTANTIATE_TEST_SUITE_P(Lines, InvalidPropertiesTest,
                         ::testing::Values(InvalidLine{"missing separator", "missing separator"},
                                           InvalidLine{"key :", "missing value"},
                                           InvalidLine{"key : \t", "missing value"},
                                           InvalidLine{": value", "missing key"},
                                           InvalidLine{" \t: value", "missing key"}));

class BrokenBuffer : public std::streambuf {
public:
    explicit BrokenBuffer(std::string prefix) : prefix(std::move(prefix)) {
        setg(this->prefix.data(), this->prefix.data(), this->prefix.data() + this->prefix.size());
    }

private:
    int_type underflow() override {
        throw std::ios_base::failure("injected read failure");
    }

    std::string prefix;
};

class PropertiesStreamTest : public ::testing::TestWithParam<std::ios::iostate> {};

TEST_P(PropertiesStreamTest, AcceptsNormalEOFWithEitherNewlinePolicyAndPreservesTheExceptionMask) {
    for (const char* text : {"", "key : value\n", "key : value", "# comment", " \t"}) {
        SCOPED_TRACE(text);
        std::istringstream input(text);
        input.exceptions(GetParam());
        Properties properties;
        EXPECT_NO_THROW(de::readProperties(input, properties));
        EXPECT_EQ(GetParam(), input.exceptions());
        if (text[0] == 'k') {
            ASSERT_TRUE(properties.hasKey("key"));
            EXPECT_EQ("value", properties.getProperty("key"));
        } else {
            EXPECT_EQ("empty properties", properties.toString());
        }
    }
}

TEST_P(PropertiesStreamTest, ReportsReadFailuresWithoutAcceptingThePartialLine) {
    BrokenBuffer buffer("valid : earlier\npartial : must not be published");
    std::istream input(&buffer);
    input.exceptions(GetParam());
    Properties properties;
    EXPECT_THROW(de::readProperties(input, properties), IOException);
    EXPECT_EQ(GetParam(), input.exceptions());
    EXPECT_EQ("valid : earlier\n", properties.toString());
}

INSTANTIATE_TEST_SUITE_P(Masks, PropertiesStreamTest,
                         ::testing::Values(std::ios::goodbit, std::ios::failbit, std::ios::badbit, std::ios::eofbit,
                                           std::ios::failbit | std::ios::badbit | std::ios::eofbit));

TEST(PropertiesParser, RejectsAnAlreadyFailedStream) {
    for (const auto state : {std::ios::failbit, std::ios::badbit, std::ios::badbit | std::ios::eofbit}) {
        std::istringstream input("key : value\n");
        input.setstate(state);
        Properties properties;
        EXPECT_THROW(de::readProperties(input, properties), IOException);
        EXPECT_FALSE(properties.hasKey("key"));
    }
}

} // namespace
