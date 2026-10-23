//----------------------------------------------------------------------
// diagnostics_test.cpp
//
// Pins the call-site diagnostics that used to be plumbed with __FILE__,
// __LINE__ and __PRETTY_FUNCTION__ and now ride a defaulted
// std::source_location (docs/build/toolchain.md, "Diagnostics without location
// macros"):
//
//   * Assert(expr) / ProtocolAssert(expr) still report the failing
//     expression text plus the file, line and enclosing function of the
//     call site, in the historical message layout.
//   * __END_CATCH / __END_CATCH_NO_RETHROW still push the enclosing
//     function onto Throwable's stack trace, and getStackTrace() still
//     formats it as one indented function per line.
//   * UnsupportedError() reports the enclosing function of the throw site
//     as its message, so an unimplemented stub throws it with no argument.
//   * Every Throwable records its throw site (file, line, function) from
//     whichever constructor built it, and toString() prints it on the line
//     after the message, before the stack trace.
//
// Both assert helpers append to a log file in the working directory, so
// the ctest entry runs this from the build tree (see tests/CMakeLists.txt).
//----------------------------------------------------------------------

#include <string>

#include <gtest/gtest.h>
#include <source_location>

#include "Assert.h"
#include "Assert1.h"
#include "Exception.h"

namespace {

// The enclosing function name recorded from inside the helpers below, so the
// expectations do not have to hard-code a mangled-looking pretty name.
std::string g_innerFunction;
unsigned g_innerLine = 0;
std::string g_outerFunction;
std::string g_swallowFunction;
std::string g_stubFunction;

void innerThrows() {
    __BEGIN_TRY

    g_innerFunction = std::source_location::current().function_name();
    g_innerLine = std::source_location::current().line() + 1;
    throw Exception("inner failed");

    __END_CATCH
}

void outerThrows() {
    __BEGIN_TRY

    g_outerFunction = std::source_location::current().function_name();
    innerThrows();

    __END_CATCH
}

void swallows() {
    __BEGIN_TRY

    g_swallowFunction = std::source_location::current().function_name();
    innerThrows();

    __END_CATCH_NO_RETHROW
}

// A free function and a virtual override in the shape of the unimplemented
// stubs: nothing is passed at the throw site.
void unsupportedStub() {
    g_stubFunction = std::source_location::current().function_name();
    throw UnsupportedError();
}

struct Stub {
    virtual ~Stub() = default;
    virtual void affect() {
        g_stubFunction = std::source_location::current().function_name();
        throw UnsupportedError();
    }
};

// The object was built on the line `here` names: same file, line and function.
void expectBuiltAt(const Throwable& t, const std::source_location& here) {
    EXPECT_STREQ(t.getThrowSite().file_name(), here.file_name()) << t.getName();
    EXPECT_EQ(t.getThrowSite().line(), here.line()) << t.getName();
    EXPECT_STREQ(t.getThrowSite().function_name(), here.function_name()) << t.getName();
}

// "thrown at <file>:<line> in <function>\n" for a throw site in this file.
std::string throwSiteLine(unsigned line, const std::string& function) {
    return "thrown at " + std::string(std::source_location::current().file_name()) + ":" + std::to_string(line) +
           " in " + function + "\n";
}

} // namespace

TEST(Diagnostics, AssertReportsCallSite) {
    const std::string function = std::source_location::current().function_name();
    unsigned line = 0;

    try {
        line = std::source_location::current().line() + 1;
        Assert(1 == 2);
        FAIL() << "Assert did not throw";
    } catch (AssertionError& e) {
        const std::string msg = e.getMessage();

        // Layout: "\nAssertion Failed : <file> : <line> : <function><expr> at <time>\n"
        // The missing separator between function and expression is historical
        // and deliberately preserved.
        EXPECT_NE(msg.find("Assertion Failed : "), std::string::npos) << msg;
        EXPECT_NE(msg.find("diagnostics_test.cpp : " + std::to_string(line) + " : "), std::string::npos) << msg;
        EXPECT_NE(msg.find(function + "1 == 2 at "), std::string::npos) << msg;
    }
}

TEST(Diagnostics, ProtocolAssertReportsCallSite) {
    const std::string function = std::source_location::current().function_name();
    unsigned line = 0;

    try {
        line = std::source_location::current().line() + 1;
        ProtocolAssert(1 == 2);
        FAIL() << "ProtocolAssert did not throw";
    } catch (InvalidProtocolException& e) {
        const std::string msg = e.getMessage();

        EXPECT_NE(msg.find("Protocol Assertion Failed : "), std::string::npos) << msg;
        EXPECT_NE(msg.find("diagnostics_test.cpp : " + std::to_string(line) + " : "), std::string::npos) << msg;
        EXPECT_NE(msg.find(function + "1 == 2 at "), std::string::npos) << msg;
    }
}

// A passing Assert must not throw, and must evaluate its expression exactly
// once - call sites do real work inside Assert().
TEST(Diagnostics, PassingAssertEvaluatesExpressionOnce) {
    int calls = 0;
    auto bump = [&calls]() { return ++calls > 0; };

    EXPECT_NO_THROW(Assert(bump()));
    EXPECT_EQ(calls, 1);

    EXPECT_NO_THROW(ProtocolAssert(bump()));
    EXPECT_EQ(calls, 2);
}

TEST(Diagnostics, EndCatchPushesEnclosingFunction) {
    try {
        innerThrows();
        FAIL() << "innerThrows did not rethrow";
    } catch (Throwable& t) {
        ASSERT_FALSE(g_innerFunction.empty());
        // One frame, indented by one space and newline-terminated.
        EXPECT_EQ(t.getStackTrace(), " " + g_innerFunction + "\n");
    }
}

TEST(Diagnostics, EndCatchNestsOutermostFirst) {
    try {
        outerThrows();
        FAIL() << "outerThrows did not rethrow";
    } catch (Throwable& t) {
        ASSERT_FALSE(g_innerFunction.empty());
        ASSERT_FALSE(g_outerFunction.empty());
        // Outermost frame first, each one space deeper than the previous.
        EXPECT_EQ(t.getStackTrace(), " " + g_outerFunction + "\n  " + g_innerFunction + "\n");
    }
}

TEST(Diagnostics, EndCatchNoRethrowSwallows) {
    EXPECT_NO_THROW(swallows());
    EXPECT_FALSE(g_swallowFunction.empty());
}

// The stack trace text is the function name only: adding file:line here would
// change every deployed log line, so it stays out of getStackTrace().
TEST(Diagnostics, StackTraceCarriesFunctionNameOnly) {
    Throwable t("boom");
    t.addStack(std::string("A::f()"));
    t.addStack(std::string("B::g()"));

    EXPECT_EQ(t.getStackTrace(), " B::g()\n  A::f()\n");
    EXPECT_EQ(t.getMessage(), "boom");
}

// An UnsupportedError thrown with no argument carries the enclosing function
// of the throw site, the same text the stack trace uses for that function.
TEST(Diagnostics, UnsupportedErrorNamesThrowSite) {
    try {
        unsupportedStub();
        FAIL() << "unsupportedStub did not throw";
    } catch (UnsupportedError& e) {
        ASSERT_FALSE(g_stubFunction.empty());
        EXPECT_EQ(e.getMessage(), g_stubFunction);
        EXPECT_EQ(e.getName(), "UnsupportedError");
    }
}

// The name is the stub's own, not the caller's - which is what makes the
// message useful in an override that only exists to reject the call.
TEST(Diagnostics, UnsupportedErrorNamesVirtualStub) {
    Stub stub;

    try {
        stub.affect();
        FAIL() << "Stub::affect did not throw";
    } catch (UnsupportedError& e) {
        ASSERT_FALSE(g_stubFunction.empty());
        EXPECT_EQ(e.getMessage(), g_stubFunction);
        EXPECT_NE(e.getMessage().find("affect"), std::string::npos) << e.getMessage();
        EXPECT_EQ(e.getMessage().find("UnsupportedErrorNamesVirtualStub"), std::string::npos) << e.getMessage();
    }
}

// A caller with something better to say still passes its own text.
TEST(Diagnostics, UnsupportedErrorKeepsExplicitMessage) {
    UnsupportedError e(std::string("storing an NPC inventory is not supported"));

    EXPECT_EQ(e.getMessage(), "storing an NPC inventory is not supported");
}

// A thrown exception records the throw expression's line, not a line of
// Exception.h, and the record survives the rethrows on the way out.
TEST(Diagnostics, ThrowSiteIsTheThrowExpression) {
    try {
        outerThrows();
        FAIL() << "outerThrows did not rethrow";
    } catch (Throwable& t) {
        ASSERT_NE(g_innerLine, 0u);
        EXPECT_NE(std::string(t.getThrowSite().file_name()).find("diagnostics_test.cpp"), std::string::npos)
            << t.getThrowSite().file_name();
        EXPECT_EQ(t.getThrowSite().line(), g_innerLine);
        EXPECT_EQ(t.getThrowSite().function_name(), g_innerFunction);
    }
}

// Every constructor records its caller, however deep the class sits under
// Throwable: the message form, inherited with `using`, and the default form
// each class declares.
TEST(Diagnostics, EveryConstructorRecordsItsCaller) {
    expectBuiltAt(Throwable("throwable"), std::source_location::current());
    expectBuiltAt(Throwable(), std::source_location::current());
    expectBuiltAt(Error("error"), std::source_location::current());
    expectBuiltAt(Error(), std::source_location::current());
    expectBuiltAt(ProtocolException("protocol"), std::source_location::current());
    expectBuiltAt(ProtocolException(), std::source_location::current());
    expectBuiltAt(EOFException("eof"), std::source_location::current());
    expectBuiltAt(EOFException(), std::source_location::current());
    expectBuiltAt(InvalidProtocolException("invalid"), std::source_location::current());
    expectBuiltAt(NoSuchElementException(), std::source_location::current());
    expectBuiltAt(PeerClosedException(), std::source_location::current());
    expectBuiltAt(InsufficientDataException(4), std::source_location::current());
    expectBuiltAt(InsufficientDataException("short", 4), std::source_location::current());
    expectBuiltAt(UnknownError("unknown", 5), std::source_location::current());
    expectBuiltAt(UnsupportedError(), std::source_location::current());
    expectBuiltAt(UnsupportedError(std::string("unsupported")), std::source_location::current());
}

// toString(): the name and message, the throw site, then the function stack.
TEST(Diagnostics, ToStringPrintsTheThrowSite) {
    try {
        outerThrows();
        FAIL() << "outerThrows did not rethrow";
    } catch (Throwable& t) {
        EXPECT_EQ(t.toString(), "Exception : inner failed\n" + throwSiteLine(g_innerLine, g_innerFunction) + " " +
                                    g_outerFunction + "\n  " + g_innerFunction + "\n");
    }
}

// The two classes that print more than the message keep their first line and
// add the throw site after it, as every other class does.
TEST(Diagnostics, ToStringOverridesPrintTheThrowSite) {
    const std::string function = std::source_location::current().function_name();
    const unsigned line = std::source_location::current().line() + 1;
    const InsufficientDataException shortBy(4);
    const InsufficientDataException shortByNothing("short");
    const UnknownError unknown("unknown", 5);

    EXPECT_EQ(shortBy.toString(), "InsufficientDataException :  (lack of 4 bytes)\n" + throwSiteLine(line, function));
    EXPECT_EQ(shortByNothing.toString(), "InsufficientDataException : short\n" + throwSiteLine(line + 1, function));
    EXPECT_EQ(unknown.toString(), "UnknownError : unknown (5) \n" + throwSiteLine(line + 2, function));
}
