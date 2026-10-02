//////////////////////////////////////////////////////////////////////
//
// pthreadAPI.cpp
//
// by Reiot, the Fallen Lord of MUDMANIA(TM)
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "pthreadAPI.h"

#include <errno.h>
#include <pthread.h>

//////////////////////////////////////////////////
//////////////////////////////////////////////////
extern int errno;


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_create()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_create_ex(pthread_t* thread, pthread_attr_t* attr, void* (*start_routine)(void*), void* arg) {
    __BEGIN_TRY

    if (pthread_create(thread, attr, start_routine, arg) < 0) {
        switch (errno) {
        case EAGAIN:
            throw ThreadException("Out of system resources, or too many threads are active.");
        default:
            throw UnknownError(strerror(errno), errno);
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_join()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_join_ex(pthread_t th, void** thread_return) {
    __BEGIN_TRY

    if (pthread_join(th, thread_return) < 0) {
        switch (errno) {
        case ESRCH:
            throw Error("The specified thread was not found.");
        case EINVAL:
            throw ThreadException("The specified thread is already detached, or another thread is already joining it.");
        case EDEADLK:
            throw Error("A thread cannot join itself.");
        default:
            throw UnknownError(strerror(errno), errno);
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_detach()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_detach_ex(pthread_t th) {
    __BEGIN_TRY

    if (pthread_detach(th) < 0) {
        switch (errno) {
        case ESRCH:
            throw Error("The specified thread was not found.");
        case EINVAL:
            throw ThreadException("The specified thread is already detached.");
        default:
            throw UnknownError(strerror(errno), errno);
        }
    }

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_exit()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_exit_ex(void* retval) {
    pthread_exit(retval);
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_self()
//
//////////////////////////////////////////////////////////////////////
pthread_t pthreadAPI::pthread_self_ex() {
    return pthread_self();
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_attr_init()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_attr_init_ex(pthread_attr_t* attr) {
    __BEGIN_TRY

    if (pthread_attr_init(attr) != 0)
        throw UnknownError();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_attr_destroy()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_attr_destroy_ex(pthread_attr_t* attr) {
    __BEGIN_TRY

    if (pthread_attr_destroy(attr) != 0)
        throw UnknownError();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_getdetachstate()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_attr_getdetachstate_ex(const pthread_attr_t* attr, int* detachstate) {
    __BEGIN_TRY

    if (pthread_attr_getdetachstate(attr, detachstate) != 0)
        throw UnknownError();

    __END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// exception version of pthread_setdetachstate()
//
//////////////////////////////////////////////////////////////////////
void pthreadAPI::pthread_attr_setdetachstate_ex(pthread_attr_t* attr, int detachstate) {
    __BEGIN_TRY

    if (pthread_attr_setdetachstate(attr, detachstate) < 0) {
        switch (errno) {
        case EINVAL:
            throw Error("invalid thread attribute state");
        default:
            throw UnknownError(strerror(errno), errno);
        }
    }

    __END_CATCH
}
