//--------------------------------------------------------------------------------
//
// Filename   : Assert.h
// Written By : Reiot
//
//--------------------------------------------------------------------------------

#ifndef __ASSERT_H__
#define __ASSERT_H__

// include files
#include <source_location>

#include "Exception.h"
#include "Types.h"

//--------------------------------------------------------------------------------
//
// Call-site diagnostics without location macros.
//
// The call site stays a macro because the diagnostic needs the unevaluated
// text of the expression (#expr) and the expression must be evaluated exactly
// once. Everything else - file, line and enclosing function - is captured by
// the defaulted std::source_location parameter, so no __FILE__ / __LINE__ /
// __PRETTY_FUNCTION__ plumbing is left here. Under Clang
// std::source_location::function_name() yields the same text
// __PRETTY_FUNCTION__ produced, so the logged message is unchanged.
//
// A failing Assert appends to assertion_failed.log in the working directory
// and throws AssertionError.
//
//--------------------------------------------------------------------------------
[[noreturn]] void assertionFailed(const char* expr,
                                  const std::source_location& loc = std::source_location::current()) noexcept(false);

//--------------------------------------------------------------------------------
//
// ProtocolAssert lets the server react when a hacked or broken client sends
// invalid data: it appends to protocol_assertion_failed.log and throws
// InvalidProtocolException, which the packet layer turns into a disconnect.
//
//--------------------------------------------------------------------------------
[[noreturn]] void
protocolAssertionFailed(const char* expr,
                        const std::source_location& loc = std::source_location::current()) noexcept(false);

// NDEBUG is never defined for this project (the top-level CMakeLists.txt
// undefines it where `zig c++` defines it itself). Assert has one meaning:
// many call sites do real work inside it, e.g.
// Assert(pTree->GetAttribute("class", iClass)), and Exception.h's
// __END_CATCH_NO_RETHROW sites depend on the handlers existing, so a build
// that defined NDEBUG would change what the servers do. It is refused here
// rather than given a second meaning. Only project translation units
// include this header: the vendored argon2 is C, which zig compiles with
// NDEBUG at -O2, but it neither includes this header nor calls assert().
#if defined(NDEBUG)
#error "NDEBUG must not be defined for DarkEden sources (see the top-level CMakeLists.txt)"
#endif

// std::source_location is portable, so the former __LINUX__ / __APPLE__ /
// __WIN_CONSOLE__ / __WIN32__ / __MFC__ ladder is gone: only __LINUX__ and
// __APPLE__ were ever defined by this build, and the remaining branches
// referenced a Windows/MFC port that no longer exists.
#define Assert(expr) ((void)((expr) ? 0 : (assertionFailed(#expr), 0)))
#define ProtocolAssert(expr) ((void)((expr) ? 0 : (protocolAssertionFailed(#expr), 0)))

#endif
