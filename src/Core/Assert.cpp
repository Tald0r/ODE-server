//--------------------------------------------------------------------------------
//
// Filename   : Assert.cpp
// Written By : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Assert.h"

#include <time.h>

#include "Exception.h"
#include "Types.h"

namespace {

//--------------------------------------------------------------------------------
//
// Appends a failed check to assertion_failed.log and returns the entry.
//
// The message layout is deliberately byte-identical to the pre-source_location
// version, including the missing separator between the function name and the
// expression text - existing logs and log readers depend on it.
//
//--------------------------------------------------------------------------------
std::string logAssertionFailure(const char* expr, const std::source_location& loc) {
    StringStream msg;

    msg << "\n"
        << "Assertion Failed : " << loc.file_name() << " : " << static_cast<uint>(loc.line());

    msg << " : " << loc.function_name();

    time_t currentTime = time(0);

    msg << expr << " at " << ctime(&currentTime);

    ofstream ofile("assertion_failed.log", ios::app);
    ofile << msg.toString() << endl;
    ofile.close();

    return msg.toString();
}

} // namespace

//--------------------------------------------------------------------------------
//
// assertionFailed
//
// This function does not need to be wrapped with __BEGIN_TRY / __END_CATCH.
//
//--------------------------------------------------------------------------------
[[noreturn]] void assertionFailed(const char* expr, const std::source_location& loc) noexcept(false) {
    throw AssertionError(logAssertionFailure(expr, loc), loc);
}

void assertionLogged(const char* expr, const std::source_location& loc) noexcept {
    logAssertionFailure(expr, loc);
}

//--------------------------------------------------------------------------------
//
// protocolAssertionFailed
//
// Idea worth considering: ban the offending user here and write a log entry
// as well.
//
//--------------------------------------------------------------------------------
[[noreturn]] void protocolAssertionFailed(const char* expr, const std::source_location& loc) noexcept(false) {
    StringStream msg;

    msg << "\n"
        << "Protocol Assertion Failed : " << loc.file_name() << " : " << static_cast<uint>(loc.line());

    msg << " : " << loc.function_name();

    time_t currentTime = time(0);

    msg << expr << " at " << ctime(&currentTime);

    ofstream ofile("protocol_assertion_failed.log", ios::app);
    ofile << msg.toString() << endl;
    ofile.close();

    throw InvalidProtocolException(msg.toString(), loc);
}
