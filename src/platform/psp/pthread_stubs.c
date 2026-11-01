/* Single-threaded pthread stubs for libgcc/libstdc++ unwinder.
   DFtoPSP vertical slice runs on one thread; these satisfy link-time deps. */
#include <stddef.h>
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef int pthread_rwlock_t;
typedef int pthread_once_t;
#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_ONCE_INIT 0
int pthread_mutex_init(pthread_mutex_t*m,const pthread_mutexattr_t*a){(void)a;*m=0;return 0;}
int pthread_mutex_destroy(pthread_mutex_t*m){(void)m;return 0;}
int pthread_mutex_lock(pthread_mutex_t*m){(void)m;return 0;}
int pthread_mutex_trylock(pthread_mutex_t*m){(void)m;return 0;}
int pthread_mutex_unlock(pthread_mutex_t*m){(void)m;return 0;}
int pthread_rwlock_init(pthread_rwlock_t*l,const void*a){(void)a;*l=0;return 0;}
int pthread_rwlock_destroy(pthread_rwlock_t*l){(void)l;return 0;}
int pthread_rwlock_rdlock(pthread_rwlock_t*l){(void)l;return 0;}
int pthread_rwlock_wrlock(pthread_rwlock_t*l){(void)l;return 0;}
int pthread_rwlock_unlock(pthread_rwlock_t*l){(void)l;return 0;}
int pthread_once(pthread_once_t*o,void(*f)(void)){if(!*o){*o=1;f();}return 0;}
int pthread_key_create(unsigned*k,void(*d)(void*)){(void)d;*k=0;return 0;}
int pthread_key_delete(unsigned k){(void)k;return 0;}
void*pthread_getspecific(unsigned k){(void)k;return NULL;}
int pthread_setspecific(unsigned k,const void*v){(void)k;(void)v;return 0;}
int pthread_init(void){return 0;}
