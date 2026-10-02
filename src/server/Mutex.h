//////////////////////////////////////////////////////////////////////////////
// Filename    : Mutex.h
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <pthread.h>

#include "Exception.h"
#include "Types.h"

// forward declaration
class MutexAttr;

//////////////////////////////////////////////////////////////////////////////
// class Mutex;
//////////////////////////////////////////////////////////////////////////////

class Mutex {
public:
    Mutex(MutexAttr* attr = NULL);
    virtual ~Mutex() noexcept;
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

public:
    string getName(void) const {
        return m_Name;
    }
    void setName(string name) {
        m_Name = name;
    }

    void lock();
    void unlock();
    void trylock();

    // The native mutex owns the thread identity, including across condition
    // waits. Default attributes refuse recursive locking and non-owner unlock.
    pthread_mutex_t* getMutex() {
        return &m_Mutex;
    }

private:
    pthread_mutex_t m_Mutex; // the mutex object
    string m_Name;           // the name of the class that owns this mutex
};

#endif
