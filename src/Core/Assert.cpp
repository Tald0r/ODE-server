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

//--------------------------------------------------------------------------------
//
// assertionFailed
//
// This function does not need to be wrapped with __BEGIN_TRY / __END_CATCH.
//
// The message layout is deliberately byte-identical to the pre-source_location
// version, including the missing separator between the function name and the
// expression text - existing logs and log readers depend on it.
//
//--------------------------------------------------------------------------------
[[noreturn]] void assertionFailed(const char* expr, const std::source_location& loc) noexcept(false) {
    StringStream msg;

    msg << "\n"
        << "Assertion Failed : " << loc.file_name() << " : " << static_cast<uint>(loc.line());

    msg << " : " << loc.function_name();

    time_t currentTime = time(0);

    msg << expr << " at " << ctime(&currentTime);

    ofstream ofile("assertion_failed.log", ios::app);
    ofile << msg.toString() << endl;
    ofile.close();

    throw AssertionError(msg.toString(), loc);
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
