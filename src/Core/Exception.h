//////////////////////////////////////////////////////////////////////
//
// Filename    : Exception.h
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__


// include files
#include <list>

#include <source_location>

#include "StringStream.h"
#include "Types.h"


//////////////////////////////////////////////////////////////////////
//
// class Throwable
//
// Base interface for exceptions and errors. Stores a message, the throw
// site and a stack trace for debugging.
//
// The throw site is the file, line and function where the object was
// constructed, which for `throw X(...)` is the throw expression: every
// constructor takes it from a defaulted std::source_location, which is
// evaluated at the caller. The derived classes inherit the message
// constructor with `using`, which keeps that default at the caller, and
// declare their default constructor themselves, because an implicit one
// would record a line of this header instead.
//
//////////////////////////////////////////////////////////////////////

class Throwable : public std::exception {
public:
    Throwable(const std::source_location& where = std::source_location::current()) : m_ThrowSite(where) {}

    Throwable(const string& message, const std::source_location& where = std::source_location::current())
        : m_Message(message), m_ThrowSite(where) {}

    // destructor
    virtual ~Throwable() {}

    // return class's name
    virtual string getName() const {
        return what();
    }

    // add function name to throwable object's function stack
    void addStack(const string& stackname) {
        m_Stacks.push_front(stackname);
    }

    // add the caller's function to the stack, without a location macro at the
    // call site: the defaulted std::source_location captures the enclosing
    // function. Under Clang its function_name() is the __PRETTY_FUNCTION__
    // text, so the stack trace format matches the macro form.
    void addStack(const std::source_location& loc = std::source_location::current()) {
        m_Stacks.push_front(loc.function_name());
    }

    // return debug string - throwable object's function stack trace
    string getStackTrace() const {
        StringStream buf;
        int i = 1;

        for (list<string>::const_iterator itr = m_Stacks.begin(); itr != m_Stacks.end(); itr++, i++) {
            for (int j = 0; j < i; j++)
                buf << " ";
            buf << *itr << '\n';
        }

        return string(buf.toString());
    }

    // get throwable object's message
    const string& getMessage() const {
        return m_Message;
    }

    // set throwable object's message
    void setMessage(const string& message) {
        m_Message = message;
    }

    // where the object was constructed
    const std::source_location& getThrowSite() const {
        return m_ThrowSite;
    }

    // "thrown at <file>:<line> in <function>", newline-terminated: the line
    // toString() prints between the message and the stack trace
    string getThrowSiteLine() const {
        StringStream buf;
        buf << "thrown at " << m_ThrowSite.file_name() << ':' << (uint)m_ThrowSite.line() << " in "
            << m_ThrowSite.function_name() << '\n';

        return buf.toString();
    }

    // return debug string - throwable object's detailed information
    virtual string toString() const {
        StringStream buf;
        buf << getName() << " : " << m_Message << '\n' << getThrowSiteLine() << getStackTrace();

        return string(buf.toString());
    }

private:
    // message string
    string m_Message;

    // where the object was constructed
    std::source_location m_ThrowSite;

    // function stack
    list<string> m_Stacks;
};

//--------------------------------------------------------------------------------
// macro definition
//--------------------------------------------------------------------------------

//--------------------------------------------------------------------------------
//
// Convenience macros for Throwable. Exception/Error messages should be
// wrapped with __BEGIN_TRY / __END_CATCH. __END_CATCH appends the
// current function name to the Throwable stack before rethrowing.
//
//--------------------------------------------------------------------------------

// No build defines NDEBUG (see the top-level CMakeLists.txt): these macros
// have one meaning, and 344 call sites use __END_CATCH_NO_RETHROW to swallow
// exceptions that would otherwise escape. A translation unit compiled with
// NDEBUG is refused rather than given a second set.
#if defined(NDEBUG)
#error "NDEBUG must not be defined for DarkEden sources (see the top-level CMakeLists.txt)"
#endif

// t.addStack() takes the enclosing function from its defaulted
// std::source_location, so these macros carry no location plumbing of their own.
#define __BEGIN_TRY try {
#define __END_CATCH         \
    }                       \
    catch (Throwable & t) { \
        t.addStack();       \
        throw;              \
    }
#define __END_CATCH_NO_RETHROW \
    }                          \
    catch (Throwable & t) {    \
        t.addStack();          \
    }


//--------------------------------------------------------------------------------
//
// critical section
//
// __ENTER_CRITICAL_SECTION(x) and __LEAVE_CRITICAL_SECTION(x) delimit a block
// that holds x for the whole of its extent. Ownership belongs to a scoped
// guard object, so the lock is released on every exit from the block: falling
// off the end, return, a goto or continue that leaves it, and a thrown object
// of any type. The previous hand-written "catch (Throwable&) { unlock(); }"
// released it only for Throwable, so a std::exception (bad_alloc,
// out_of_range) left the mutex held, and a return skipped the unlock unless
// the author remembered to write one by hand.
//
// x may be any BasicLockable: it only has to expose lock() and unlock().
// Mutex, Zone, ZoneGroup, PCFinder and ObjectRegistry all qualify. The guard
// calls exactly those two members and nothing else, so ZoneGroup keeps
// recording its owning thread for the DE_OWNERSHIP_CHECKS tripwire.
//
// x is evaluated once, at __ENTER_CRITICAL_SECTION, and the guard holds its
// address from then on. The argument of __LEAVE_CRITICAL_SECTION is unused and
// kept only so both ends of a block stay readable.
//
// Never call x.unlock() by hand inside the block: the guard would unlock a
// second time when it is destroyed, and unlocking an already unlocked
// non-recursive pthread mutex is undefined. To run work outside the lock while
// staying inside the block, use the guard, which tracks ownership:
//
//     __CRITICAL_SECTION_LOCK.unlock();   // release early
//     ... work that must not hold the lock ...
//     __CRITICAL_SECTION_LOCK.lock();     // optional: take it again
//
// tests/critical_section_test.cpp pins this behaviour;
// tests/tools/critical_section_audit.pl reports manual lock/unlock calls that
// slipped back inside a block.
//
//--------------------------------------------------------------------------------

template <typename Lockable> class CriticalSection {
public:
    explicit CriticalSection(Lockable& lockable) : m_pLockable(&lockable), m_Owns(false) {
        // Lock before the guard owns anything, as the old macro did: if lock()
        // throws there is no guard yet and nothing is unlocked.
        m_pLockable->lock();
        m_Owns = true;
    }

    ~CriticalSection() {
        if (!m_Owns)
            return;

        m_Owns = false;

        try {
            m_pLockable->unlock();
        } catch (...) {
            // A destructor must not throw. unlock() fails only on a
            // programming error (releasing a mutex this thread does not hold),
            // and letting that escape while another exception is already in
            // flight would call std::terminate and lose the original
            // diagnostic. Mutex::~Mutex swallows the same failure.
        }
    }

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    // Release the lock before the end of the block. The destructor then does
    // nothing, so an early release can never unlock twice.
    void unlock() {
        if (m_Owns) {
            m_Owns = false;
            m_pLockable->unlock();
        }
    }

    // Take the lock again inside the same block.
    void lock() {
        if (!m_Owns) {
            m_pLockable->lock();
            m_Owns = true;
        }
    }

    bool ownsLock() const {
        return m_Owns;
    }

private:
    Lockable* m_pLockable;
    bool m_Owns;
};

// Name of the guard that __ENTER_CRITICAL_SECTION declares. Deliberately
// public: it is the only supported way to drop or retake the lock inside a
// critical section.
#define __CRITICAL_SECTION_LOCK deCriticalSectionLock

#define __ENTER_CRITICAL_SECTION(mutex) \
    {                                   \
        CriticalSection __CRITICAL_SECTION_LOCK{mutex};

#define __LEAVE_CRITICAL_SECTION(mutex) }

//--------------------------------------------------------------------------------
//
// cout debugging
//
//--------------------------------------------------------------------------------
// Only the console form survives: this build defines __LINUX__ (or __APPLE__),
// never __WIN32__, __WIN_CONSOLE__ or __MFC__, so the Windows and MFC branches
// were dead code referencing a port that no longer exists.
#define __BEGIN_DEBUG try {
#define __END_DEBUG                   \
    }                                 \
    catch (Throwable & t) {           \
        cout << t.toString() << endl; \
        throw;                        \
    }                                 \
    catch (exception & e) {           \
        cout << e.what() << endl;     \
        throw;                        \
    }


//////////////////////////////////////////////////////////////////////
//
// Exception
//
//////////////////////////////////////////////////////////////////////
class Exception : public Throwable {
public:
    using Throwable::Throwable;
    Exception(const std::source_location& where = std::source_location::current()) : Throwable(where) {}
    string getName() const {
        return "Exception";
    }
};

//////////////////////////////////////////////////////////////////////
//
// I/O Exception
//
// Errors that can occur during read/write or IPC operations.
//
//////////////////////////////////////////////////////////////////////
// Errors that can occur during read/write or IPC operations.
class IOException : public Exception {
public:
    using Exception::Exception;
    IOException(const std::source_location& where = std::source_location::current()) : Exception(where) {}
    string getName() const {
        return "IOException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Non Blocking I/O Exception
//
// I/O would block when running in nonblocking mode.
//
//////////////////////////////////////////////////////////////////////
class NonBlockingIOException : public IOException {
public:
    using IOException::IOException;
    NonBlockingIOException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "NonBlockingIOException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Interrupted I/O Exception
//
// I/O operation was interrupted by a signal.
//
//////////////////////////////////////////////////////////////////////
class InterruptedIOException : public IOException {
public:
    using IOException::IOException;
    InterruptedIOException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "InterruptedIOException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// EOF Exception
//
// Unexpected EOF during an I/O operation.
//
//////////////////////////////////////////////////////////////////////
class EOFException : public IOException {
public:
    using IOException::IOException;
    EOFException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "EOFException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// File Not Opened Exception
//
//////////////////////////////////////////////////////////////////////
class FileNotOpenedException : public IOException {
public:
    using IOException::IOException;
    FileNotOpenedException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "FileNotOpenedException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// File Already Exist Exception
//
//////////////////////////////////////////////////////////////////////
class FileAlreadyExistException : public IOException {
public:
    using IOException::IOException;
    FileAlreadyExistException(const std::source_location& where = std::source_location::current())
        : IOException(where) {}
    string getName() const {
        return "FileAlreadyExistException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// File Not Exist Exception
//
//////////////////////////////////////////////////////////////////////
class FileNotExistException : public IOException {
public:
    using IOException::IOException;
    FileNotExistException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "FileNotExistException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Time out Exception
//
// Operation exceeded the allowed time.
//
//////////////////////////////////////////////////////////////////////
class TimeoutException : public IOException {
public:
    using IOException::IOException;
    TimeoutException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "TimeoutException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Socket Exception
//
// Generic socket-related errors.
//
//////////////////////////////////////////////////////////////////////
class SocketException : public IOException {
public:
    using IOException::IOException;
    SocketException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "SocketException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Bind Exception
//
// Errors raised while calling bind().
//
//////////////////////////////////////////////////////////////////////
class BindException : public SocketException {
public:
    using SocketException::SocketException;
    BindException(const std::source_location& where = std::source_location::current()) : SocketException(where) {}
    string getName() const {
        return "BindException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Connect Exception
//
// Unable to connect to the remote host (likely not listening).
//
//////////////////////////////////////////////////////////////////////
class ConnectException : public SocketException {
public:
    using SocketException::SocketException;
    ConnectException(const std::source_location& where = std::source_location::current()) : SocketException(where) {}
    string getName() const {
        return "ConnectException";
    }
};

// A peer's orderly TCP close is distinct from resets and other I/O failures.
// Existing connection cleanup still catches this through ConnectException.
class PeerClosedException : public ConnectException {
public:
    PeerClosedException(const std::source_location& where = std::source_location::current())
        : ConnectException("connect closed.", where) {}
    string getName() const override {
        return "PeerClosedException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Protocol Exception
//
// Errors while parsing or handling a packet.
//
//////////////////////////////////////////////////////////////////////
class ProtocolException : public IOException {
public:
    using IOException::IOException;
    ProtocolException(const std::source_location& where = std::source_location::current()) : IOException(where) {}
    string getName() const {
        return "ProtocolException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Idle Exception
//
// No input from the peer for an extended period.
//
//////////////////////////////////////////////////////////////////////
class IdleException : public ProtocolException {
public:
    using ProtocolException::ProtocolException;
    IdleException(const std::source_location& where = std::source_location::current()) : ProtocolException(where) {}
    string getName() const {
        return "IdleException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Invalid Protocol Exception
//
// Malformed protocol payload.
//
//////////////////////////////////////////////////////////////////////
class InvalidProtocolException : public ProtocolException {
public:
    using ProtocolException::ProtocolException;
    InvalidProtocolException(const std::source_location& where = std::source_location::current())
        : ProtocolException(where) {}
    string getName() const {
        return "InvalidProtocolException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Insufficient Data Exception
//
// Packet data is too short to continue processing.
//
//////////////////////////////////////////////////////////////////////
class InsufficientDataException : public ProtocolException {
public:
    InsufficientDataException(uint size = 0, const std::source_location& where = std::source_location::current())
        : ProtocolException(where), m_Size(size) {}
    InsufficientDataException(const string& msg, uint size = 0,
                              const std::source_location& where = std::source_location::current())
        : ProtocolException(msg, where), m_Size(size) {}
    string getName() const {
        return "InsufficientDataException";
    }
    uint getSize() const {
        return m_Size;
    }
    string toString() const {
        StringStream buf;
        buf << getName() << " : " << getMessage();
        if (m_Size > 0) {
            buf << " (lack of " << m_Size << " bytes)";
        }
        buf << '\n' << getThrowSiteLine() << getStackTrace();

        return buf.toString();
    }

private:
    uint m_Size;
};

//////////////////////////////////////////////////////////////////////
//
// Use when network or system issues require disconnecting the client.
//
//////////////////////////////////////////////////////////////////////
class DisconnectException : public ProtocolException {
public:
    using ProtocolException::ProtocolException;
    DisconnectException(const std::source_location& where = std::source_location::current())
        : ProtocolException(where) {}
    string getName() const {
        return "DisconnectException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Ignore packets that must be skipped in special cases.
//
//////////////////////////////////////////////////////////////////////
class IgnorePacketException : public ProtocolException {
public:
    using ProtocolException::ProtocolException;
    IgnorePacketException(const std::source_location& where = std::source_location::current())
        : ProtocolException(where) {}
    string getName() const {
        return "IgnorePacketException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Thread Exception
//
// Exceptions raised during thread or synchronization operations.
//
//////////////////////////////////////////////////////////////////////
class ThreadException : public Exception {
public:
    using Exception::Exception;
    ThreadException(const std::source_location& where = std::source_location::current()) : Exception(where) {}
    string getName() const {
        return "ThreadException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Mutex Exception
//
// Exceptions from mutex locking/unlocking.
//
//////////////////////////////////////////////////////////////////////
class MutexException : public ThreadException {
public:
    using ThreadException::ThreadException;
    MutexException(const std::source_location& where = std::source_location::current()) : ThreadException(where) {}
    string getName() const {
        return "MutexException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Mutex Attribute Exception
//
// Exceptions related to mutex attributes.
//
//////////////////////////////////////////////////////////////////////
class MutexAttrException : public MutexException {
public:
    using MutexException::MutexException;
    MutexAttrException(const std::source_location& where = std::source_location::current()) : MutexException(where) {}
    string getName() const {
        return "MutexAttrException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Conditional Variable Exception
//
// Exceptions from condition variable operations.
//
//////////////////////////////////////////////////////////////////////
class CondVarException : public ThreadException {
public:
    using ThreadException::ThreadException;
    CondVarException(const std::source_location& where = std::source_location::current()) : ThreadException(where) {}
    string getName() const {
        return "CondVarException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Semaphore Exception
//
// Exceptions from semaphore operations.
//
//////////////////////////////////////////////////////////////////////
class SemaphoreException : public ThreadException {
public:
    using ThreadException::ThreadException;
    SemaphoreException(const std::source_location& where = std::source_location::current()) : ThreadException(where) {}
    string getName() const {
        return "SemaphoreException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// SQL Exception
//
// SQL execution errors.
//
//////////////////////////////////////////////////////////////////////
class SQLException : public Exception {
public:
    using Exception::Exception;
    SQLException(const std::source_location& where = std::source_location::current()) : Exception(where) {}
    string getName() const {
        return "SQLException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// SQL Warning
//
// Non-fatal SQL warnings.
//
//////////////////////////////////////////////////////////////////////
class SQLWarning : public SQLException {
public:
    using SQLException::SQLException;
    SQLWarning(const std::source_location& where = std::source_location::current()) : SQLException(where) {}
    string getName() const {
        return "SQLWarning";
    }
};


//////////////////////////////////////////////////////////////////////
//
// SQL Connect Exception
//
// Failed to connect to SQL; connection attempt did not reach the server.
//
//////////////////////////////////////////////////////////////////////
class SQLConnectException : public SQLException {
public:
    using SQLException::SQLException;
    SQLConnectException(const std::source_location& where = std::source_location::current()) : SQLException(where) {}
    string getName() const {
        return "SQLConnectException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Query Exception
//
//////////////////////////////////////////////////////////////////////
class SQLQueryException : public SQLException {
public:
    using SQLException::SQLException;
    SQLQueryException(const std::source_location& where = std::source_location::current()) : SQLException(where) {}
    string getName() const {
        return "SQLQueryException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Runtime Exception
//
// Generic runtime errors for miscellaneous failure cases.
//
//////////////////////////////////////////////////////////////////////
class RuntimeException : public Exception {
public:
    using Exception::Exception;
    RuntimeException(const std::source_location& where = std::source_location::current()) : Exception(where) {}
    string getName() const {
        return "RuntimeException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Invalid Arguemnt Exception
//
// Function or method received an invalid argument.
//
//////////////////////////////////////////////////////////////////////
class InvalidArgumentException : public RuntimeException {
public:
    using RuntimeException::RuntimeException;
    InvalidArgumentException(const std::source_location& where = std::source_location::current())
        : RuntimeException(where) {}
    string getName() const {
        return "InvalidArgumentException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Out Of Bound Exception
//
// Index or coordinate out of bounds.
//
//////////////////////////////////////////////////////////////////////
class OutOfBoundException : public RuntimeException {
public:
    using RuntimeException::RuntimeException;
    OutOfBoundException(const std::source_location& where = std::source_location::current())
        : RuntimeException(where) {}
    string getName() const {
        return "OutOfBoundException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Interrupted Exception
//
// System call interrupted by a signal.
//
//////////////////////////////////////////////////////////////////////
class InterruptedException : public RuntimeException {
public:
    using RuntimeException::RuntimeException;
    InterruptedException(const std::source_location& where = std::source_location::current())
        : RuntimeException(where) {}
    string getName() const {
        return "InterruptedException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// No Such Element Exception
//
// Lookup failed to find an element for the requested key.
//
//////////////////////////////////////////////////////////////////////
class NoSuchElementException : public RuntimeException {
public:
    using RuntimeException::RuntimeException;
    NoSuchElementException(const std::source_location& where = std::source_location::current())
        : RuntimeException(where) {}
    string getName() const {
        return "NoSuchElementException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Duplicated Exception
//
// Duplicate key detected.
//
//////////////////////////////////////////////////////////////////////
class DuplicatedException : public RuntimeException {
public:
    using RuntimeException::RuntimeException;
    DuplicatedException(const std::source_location& where = std::source_location::current())
        : RuntimeException(where) {}
    string getName() const {
        return "DuplicatedException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Game Exception
//
// Game-specific exceptions used for control-flow style handling.
//
//////////////////////////////////////////////////////////////////////
class GameException : public Exception {
public:
    using Exception::Exception;
    GameException(const std::source_location& where = std::source_location::current()) : Exception(where) {}
    string getName() const {
        return "GameException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Portal Exception
//
// PC failed to move through a portal.
//
//////////////////////////////////////////////////////////////////////
class PortalException : public GameException {
public:
    using GameException::GameException;
    PortalException(const std::source_location& where = std::source_location::current()) : GameException(where) {}
    string getName() const {
        return "PortalException";
    }
};

//////////////////////////////////////////////////////////////////////
//
// No suitable empty tile was found when searching for placement.
//
//////////////////////////////////////////////////////////////////////
class EmptyTileNotExistException : public GameException {
public:
    using GameException::GameException;
    EmptyTileNotExistException(const std::source_location& where = std::source_location::current())
        : GameException(where) {}
    string getName() const {
        return "EmptyTileNotExistException";
    }
};

//////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////
class InventoryFullException : public GameException {
public:
    using GameException::GameException;
    InventoryFullException(const std::source_location& where = std::source_location::current())
        : GameException(where) {}
    string getName() const {
        return "InventoryFullException";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Error
//
//////////////////////////////////////////////////////////////////////
class Error : public Throwable {
public:
    using Throwable::Throwable;
    Error(const std::source_location& where = std::source_location::current()) : Throwable(where) {}
    string getName() const {
        return "Error";
    }
};
//////////////////////////////////////////////////////////////////////
//
// Game Error
//
//////////////////////////////////////////////////////////////////////
class GameError : public Error {
public:
    using Error::Error;
    GameError(const std::source_location& where = std::source_location::current()) : Error(where) {}
    string getName() const {
        return "GameError";
    }
};


//////////////////////////////////////////////////////////////////////
//
// Assertion Error
//
//////////////////////////////////////////////////////////////////////
class AssertionError : public Error {
public:
    using Error::Error;
    AssertionError(const std::source_location& where = std::source_location::current()) : Error(where) {}
    string getName() const {
        return "AssertionError";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Unsupported Error
//
//////////////////////////////////////////////////////////////////////
class UnsupportedError : public Error {
public:
    // The message is the enclosing function of the throw site, captured by
    // the defaulted std::source_location. Under Clang function_name() is the
    // same text __PRETTY_FUNCTION__ produces, so the message reads like the
    // function names in a stack trace.
    UnsupportedError(const std::source_location& where = std::source_location::current())
        : Error(where.function_name(), where) {}
    using Error::Error;
    string getName() const {
        return "UnsupportedError";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Log Error
//
// Ordinary errors should be logged with LogError to the system log.
// (Consider refining the logging strategy.)
//
//////////////////////////////////////////////////////////////////////
class LogError : public Error {
public:
    using Error::Error;
    LogError(const std::source_location& where = std::source_location::current()) : Error(where) {}
    string getName() const {
        return "LogError";
    }
};

//////////////////////////////////////////////////////////////////////
//
// Unknown Error
//
//////////////////////////////////////////////////////////////////////
class UnknownError : public Error {
public:
    using Error::Error;
    UnknownError(const std::source_location& where = std::source_location::current()) : Error(where) {}
    UnknownError(const string& msg, uint ErrorCode, const std::source_location& where = std::source_location::current())
        : Error(msg, where), m_ErrorCode(ErrorCode) {}
    string getName() const {
        return "UnknownError";
    }
    uint getErrorCode() const {
        return m_ErrorCode;
    }
    string toString() const {
        StringStream buf;
        buf << getName() << " : " << getMessage() << " (" << getErrorCode() << ") \n"
            << getThrowSiteLine() << getStackTrace();

        return buf.toString();
    }

private:
    uint m_ErrorCode;
};

#define __BEGIN_DEBUG_EX try {
#define __END_DEBUG_EX                                              \
    }                                                               \
    catch (ProtocolException&) {                                    \
        throw;                                                      \
    }                                                               \
    catch (Error&) {                                                \
        throw;                                                      \
    }                                                               \
    catch (Exception & e) {                                         \
        ofstream file("packet_exception.txt", ios::out | ios::app); \
        file << e.toString() << endl;                               \
        file.close();                                               \
        cout << e.toString() << endl;                               \
    }

#endif
