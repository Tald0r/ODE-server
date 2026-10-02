//////////////////////////////////////////////////////////////////////
//
// pthreadAPI.h
//
// by Reiot
//
//////////////////////////////////////////////////////////////////////

#ifndef __PTHREAD_API_H__
#define __PTHREAD_API_H__

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include <pthread.h>

#include "Exception.h"


//////////////////////////////////////////////////
// type redefinition
//////////////////////////////////////////////////
typedef pthread_t TID;


namespace pthreadAPI {
//
// exception version of pthread_mutex_init()
//
void pthread_mutex_init_ex(pthread_mutex_t* mutex, const pthread_mutexattr_t* mutexattr);

//
// exception version of pthread_mutex_destroy()
//
void pthread_mutex_destroy_ex(pthread_mutex_t* mutex);

//
// exception version of pthread_mutex_lock()
//
void pthread_mutex_lock_ex(pthread_mutex_t* mutex);

//
// exception version of pthread_mutex_unlock()
//
void pthread_mutex_unlock_ex(pthread_mutex_t* mutex);

//
// exception version of pthread_mutex_trylock()
//
void pthread_mutex_trylock_ex(pthread_mutex_t* mutex);

//
// exception version of pthread_mutexattr_init()
//
void pthread_mutexattr_init_ex(pthread_mutexattr_t* attr);

//
// exception version of pthread_mutexattr_destroy()
//
void pthread_mutexattr_destroy_ex(pthread_mutexattr_t* attr);


//
// exception version of pthread_cond_init()
//
void pthread_cond_init_ex(pthread_cond_t* cond, pthread_condattr_t* cond_attr);

//
// exception version of pthread_cond_destroy()
//
void pthread_cond_destroy_ex(pthread_cond_t* cond);

//
// exception version of pthread_cond_signal()
//
void pthread_cond_signal_ex(pthread_cond_t* cond);

//
// exception version of pthread_cond_wait()
//
void pthread_cond_wait_ex(pthread_cond_t* cond, pthread_mutex_t* mutex);

//
// exception version of pthread_cond_timedwait()
//
void pthread_cond_timedwait_ex(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime);

//
// exception version of pthread_cond_broadcast()
//
void pthread_cond_broadcast_ex(pthread_cond_t* cond);

//
// exception version of pthread_condattr_init()
//
void pthread_condattr_init_ex(pthread_condattr_t* attr);

//
// exception version of pthread_condattr_destroy()
//
void pthread_condattr_destroy_ex(pthread_condattr_t* attr);
} // end of namespace pthreadAPI

#endif
