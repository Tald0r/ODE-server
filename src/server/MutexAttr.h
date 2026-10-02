//////////////////////////////////////////////////////////////////////
//
// MutexAttr.h
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////
//
// Mutex-Attribute Class
//
// The Mutex-Attribute class is used as the parameter to pthread_mutex_init()
// when several Mutex objects with the same attribute have to be created.
// That is, only one Mutex-Attribute object has to be created for
// all of them.
//
//////////////////////////////////////////////////////////////////////


#ifndef __MUTEX_ATTR_H__
#define __MUTEX_ATTR_H__

//////////////////////////////////////////////////
// include
//////////////////////////////////////////////////
#include <pthread.h>

#include <cstring>

#include "Exception.h"
#include "Types.h"
#include "pthreadAPI.h"


//////////////////////////////////////////////////
// forward declaration
//////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// class MutexAttr;
//
//////////////////////////////////////////////////////////////////////

class MutexAttr {
    //////////////////////////////////////////////////
    // constructor / destructor
    //////////////////////////////////////////////////

public:
    // constructor
    MutexAttr() {
        pthreadAPI::pthread_mutexattr_init_ex(&m_Attr);
        const int result = pthread_mutexattr_settype(&m_Attr, PTHREAD_MUTEX_ERRORCHECK);
        if (result != 0) {
            pthread_mutexattr_destroy(&m_Attr);
            throw UnknownError(std::strerror(result), result);
        }
    }

    // destructor
    ~MutexAttr() {
        pthread_mutexattr_destroy(&m_Attr);
    }
    MutexAttr(const MutexAttr&) = delete;
    MutexAttr& operator=(const MutexAttr&) = delete;


    //////////////////////////////////////////////////
    // public methods
    //////////////////////////////////////////////////

public:
    //
    // return mutex-attribute object
    //
    // *CAUTION*
    //
    // do not return pthread_mutexattr_t value !!
    // use pthread_mutexattr_t pointer instead.
    // (assignment may not be supported for
    // pthread_mutexattr_t)
    //
    // Attributes default to ERRORCHECK. Explicit native changes (for example
    // RECURSIVE) are honored by Mutex; the caller owns their synchronization.
    pthread_mutexattr_t* getAttr() {
        return &m_Attr;
    }


    //////////////////////////////////////////////////
    // attributes
    //////////////////////////////////////////////////

private:
    // mutex attribute
    pthread_mutexattr_t m_Attr;
};

#endif
