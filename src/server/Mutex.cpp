//////////////////////////////////////////////////////////////////////////////
// Filename    : Mutex.cpp
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Mutex.h"

#include "MutexAttr.h"
#include "StringStream.h"
#include "Utility.h"
#include "pthreadAPI.h"

namespace {
[[noreturn]] void reportMutexFailure(const char* operation, const std::string& name, const MutexException& error) {
    const auto message = std::string("Mutex::") + operation + " [" + name + "]: " + error.toString();
    std::cerr << message << std::endl;
    filelog("MutexError.log", "%s", message.c_str());
    throw Error(message);
}
} // namespace

////////////////////////////////////////////////////////////////////////////////
//
// constructor
//
////////////////////////////////////////////////////////////////////////////////

Mutex::Mutex(MutexAttr* attr) {
    __BEGIN_TRY

    if (attr) {
        pthreadAPI::pthread_mutex_init_ex(&m_Mutex, attr->getAttr());
    } else {
        MutexAttr defaults;
        pthreadAPI::pthread_mutex_init_ex(&m_Mutex, defaults.getAttr());
    }

    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
//
// destructor
//
////////////////////////////////////////////////////////////////////////////////

Mutex::~Mutex() noexcept {
    try {
        pthreadAPI::pthread_mutex_destroy_ex(&m_Mutex);
    } catch (const MutexException& me) {
        cerr << me.toString() << endl;
    } catch (...) {
        // must not throw from destructor
    }
}


////////////////////////////////////////////////////////////////////////////////
//
// lock mutex
//
////////////////////////////////////////////////////////////////////////////////
void Mutex::lock() {
    __BEGIN_TRY


    try {
        pthreadAPI::pthread_mutex_lock_ex(&m_Mutex);
    } catch (MutexException& me) {
        reportMutexFailure("lock", m_Name, me);
    }


    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
//
// if mutex is locked, throw MutexException. else lock mutex
//
////////////////////////////////////////////////////////////////////////////////
void Mutex::trylock() {
    __BEGIN_TRY


    try {
        pthreadAPI::pthread_mutex_trylock_ex(&m_Mutex);
    } catch (MutexException& me) {
        reportMutexFailure("trylock", m_Name, me);
    }


    __END_CATCH
}


////////////////////////////////////////////////////////////////////////////////
//
// unlock mutex
//
////////////////////////////////////////////////////////////////////////////////
void Mutex::unlock() {
    __BEGIN_TRY

    try {
        pthreadAPI::pthread_mutex_unlock_ex(&m_Mutex);
    } catch (MutexException& me) {
        reportMutexFailure("unlock", m_Name, me);
    }


    __END_CATCH
}
