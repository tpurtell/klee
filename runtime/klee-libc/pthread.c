#include "pthread.h"
#include <errno.h>

void* malloc(size_t s);
void *memset(void * dst, int s, size_t count);

// int __thread_create(... /* function pointer, arg0, arg1, ... argN */);
int __thread_alive(int tid);
int __thread_tid();
void __thread_detach(int tid);
void __thread_yield();

typedef struct  {
  void* next;
  pthread_t tid;
  void* ret;
} pthread_list_entry;
#define NULL 0
pthread_list_entry *__pthread_tids = NULL;

void __pthread_trampoline(void *(*f) (void *), void *a) 
{
  void* ret = f(a);
  pthread_list_entry* e = __pthread_tids;
  while(e != NULL && e->tid != __thread_tid())
    e = (pthread_list_entry*)e->next;
  if(e) {
    e->ret = ret;
  }
}

int    pthread_create(pthread_t *t, const pthread_attr_t *o,
     void *(*f) (void *), void *a)
{
  if(!t) {
    klee_warning("ignoring (EINVAL for pthread_create)");
    return EINVAL;
  } 
  pthread_list_entry* e = (pthread_list_entry*)malloc(sizeof(pthread_list_entry));
  e->next = __pthread_tids;
  e->ret = NULL;
  e->tid = __thread_create(__pthread_trampoline, f, a);
  if(t)
    *t = e->tid;
  __pthread_tids = e;
  if(o && o->detached)
    pthread_detach(e->tid);
  return 0;
}

int    pthread_detach(pthread_t t)
{
  __thread_detach(t);
  pthread_list_entry* e = __pthread_tids;
  if(e->tid == t) {
    __pthread_tids = (pthread_list_entry*)e->next;
    free(e);
  } else {
    while(e != NULL && e->next != NULL && ((pthread_list_entry*)e->next)->tid != t)
      e = (pthread_list_entry*)e->next;
    if(e->next && ((pthread_list_entry*)e->next)->tid == t) {
      pthread_list_entry* d = e;
      e->next = d->next;
      free(d);
    } else {
      klee_warning("ignoring (EINVAL for pthread_detach)");
      return EINVAL;
    }
  }
  return 0;
}

int    _pthread_join(pthread_t t, void **ret)
{
  return pthread_join(t, ret);
}
int    pthread_join(pthread_t t, void **ret)
{
  while(__thread_alive(t)) {
    __thread_yield();
  }
  __thread_detach(t);
  pthread_list_entry* e = __pthread_tids;
  if(e && e->tid == t) {
    __pthread_tids = (pthread_list_entry*)e->next;
    free(e);
  } else {
    while(e != NULL && e->next != NULL && ((pthread_list_entry*)e->next)->tid != t)
      e = (pthread_list_entry*)e->next;
    if(e && e->next && ((pthread_list_entry*)e->next)->tid == t) {
      pthread_list_entry* d = e->next;
      e->next = d->next;
      if(ret)
        *ret = d->ret;
      free(d);
    } else {
      klee_warning("ignoring (EINVAL for pthread_detach)");
      return EINVAL;
    }
  }
  return 0;
}

pthread_t  pthread_self(void)
{
  return __thread_tid();
}


int    pthread_mutex_destroy(pthread_mutex_t *m)
{
  if(m->owner != 0) {
    klee_warning("pthread_mutex_destroy destroying locked mutex");
  }
  return 0;
}

int    pthread_mutex_init(pthread_mutex_t *m,
     const pthread_mutexattr_t *a)
{
  memset(m, 0, sizeof(*m));
  if(a)
    m->attr = *a;
  else 
    pthread_mutexattr_init(&m->attr);
  return 0;
}
int    pthread_mutex_lock(pthread_mutex_t *m)
{
  pthread_t t = __thread_tid();
  if(m->attr.recursive) {
    if(m->owner != t) {
      while(m->owner != 0) {
        __thread_yield();
      }
    }
    ++m->count;
  } else {
    while(m->owner != 0) {
      __thread_yield();
    }
    m->count = 1;
  }
  m->owner = t;
  return 0;
}
int    pthread_mutex_trylock(pthread_mutex_t *m)
{
  pthread_t t = __thread_tid();
  if(m->attr.recursive) {
    if(m->owner != t) {
      return EBUSY;
    }
    ++m->count;
  } else {
    if(m->owner != 0)
      return EBUSY;
    m->count = 1;
  }
  m->owner = t;
  return 0;
}
int    pthread_mutex_timedlock(pthread_mutex_t *m,
     const struct timespec *ts)
{
  //TODO: convert from ts;
  int limit = 50;
  pthread_t t = __thread_tid();
  if(m->attr.recursive) {
    if(m->owner != t) {
      while(m->owner != 0 && --limit) {
        __thread_yield();
      }
    }
    if(m->owner != 0 && limit == 0)
      return ETIMEDOUT;
    ++m->count;
  } else {
    while(m->owner != 0 && --limit) {
      __thread_yield();
    }
    if(m->owner != 0 && limit == 0)
      return ETIMEDOUT;
    m->count = 1;
  }
  m->owner = t;
  return 0;
}
int    pthread_mutex_unlock(pthread_mutex_t *m)
{
  if(m->owner != __thread_tid()) {
    klee_warning("pthread_mutex_unlock called by non owner");
  }
  --m->count;
  if(m->count == 0) {
      m->owner = 0;
  }
  return 0;
}

int    pthread_attr_getinheritsched(const pthread_attr_t *o, int *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getschedparam(const pthread_attr_t *o,
     struct sched_param *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getschedpolicy(const pthread_attr_t *o, int *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getscope(const pthread_attr_t *o, int *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setinheritsched(pthread_attr_t *o, int s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setschedparam(pthread_attr_t *o,
     const struct sched_param *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setschedpolicy(pthread_attr_t *o, int s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setscope(pthread_attr_t *o, int s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_getschedparam(pthread_t t, int *s,
     struct sched_param *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_setschedparam(pthread_t t, int s,
     const struct sched_param *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int __pthread_concurrency = 1;
int    pthread_getconcurrency(void)
{
  return __pthread_concurrency;
}

int    pthread_setconcurrency(int c)
{
  __pthread_concurrency = c;
  return 0;
}

int    pthread_mutexattr_init(pthread_mutexattr_t *a)
{
  memset(a, 0, sizeof(*a));
  return 0;
}

int    pthread_mutexattr_destroy(pthread_mutexattr_t *a)
{
  return 0;
}

int    pthread_mutexattr_gettype(pthread_mutexattr_t *a, int *t)
{
  if(!a) {
    klee_warning("ignoring (EINVAL for missing attr for pthread_mutexattr_gettype)");
    return EINVAL;
  }
  if(!t) {
    klee_warning("ignoring (EINVAL for pthread_mutexattr_gettype)");
    return EINVAL;
  } 
  *t = a->recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL;
  return 0;
}

int    pthread_mutexattr_getpshared(const pthread_mutexattr_t *a,
     int *s)
{
  *s = PTHREAD_PROCESS_PRIVATE;
  return 0;
}

int    pthread_mutexattr_settype(pthread_mutexattr_t *a, int t)
{
  if(!a) {
    klee_warning("ignoring (EINVAL for missing attr for pthread_mutexattr_settype)");
    return EINVAL;
  }
  switch(t) {
  case PTHREAD_MUTEX_RECURSIVE:
    a->recursive = 1;
    break;
  case PTHREAD_MUTEX_NORMAL:
  case PTHREAD_MUTEX_ERRORCHECK:
    a->recursive = 0;
    break;
  default:
    klee_warning("ignoring (EINVAL for bad type for pthread_mutexattr_settype)");
    return EINVAL;
  }
  return 0;
}

int    pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int s)
{
  if(s == PTHREAD_PROCESS_SHARED) {
    klee_warning("process shared mutex not supported");
    return EINVAL;
  }
  return 0;
}

int    pthread_mutexattr_getprioceiling(pthread_mutexattr_t *a,
     int *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a,
     int p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_mutex_getprioceiling(pthread_mutex_t *m, int *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_mutex_setprioceiling(pthread_mutex_t *m, int s, int *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}


int    pthread_mutexattr_getprotocol(pthread_mutexattr_t *a, int *p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int p)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}


int    pthread_attr_destroy(pthread_attr_t *a)
{
  return 0;
}

int    pthread_attr_getstack(const pthread_attr_t *a __restrict, 
     void ** __restrict stackaddr, 
     size_t * __restrict stacksize)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getstacksize(const pthread_attr_t *a, size_t *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getguardsize(const pthread_attr_t *a, size_t *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getstackaddr(const pthread_attr_t *a, void **s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_getdetachstate(const pthread_attr_t *a, int *t)
{
  if(!a) {
    klee_warning("ignoring (EINVAL for missing attr for pthread_attr_getdetachstate)");
    return EINVAL;
  }
  if(!t) {
    klee_warning("ignoring (EINVAL for pthread_attr_getdetachstate)");
    return EINVAL;
  } 
  *t = a->detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
  return 0;
}

int    pthread_attr_init(pthread_attr_t *a)
{
  memset(a, 0, sizeof(*a));
  return 0;
}

int    pthread_attr_setstacksize(pthread_attr_t *a, size_t s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setguardsize(pthread_attr_t *a, size_t s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setstack(pthread_attr_t *a, void *p, size_t s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setstackaddr(pthread_attr_t *a, void *s)
{
  klee_warning("%s not supported", __FUNCTION__);
  return EINVAL;
}

int    pthread_attr_setdetachstate(pthread_attr_t *a, int p)
{
  if(!a) {
    klee_warning("ignoring (EINVAL for missing attr for pthread_attr_setdetachstate)");
    return EINVAL;
  }
  switch(p) {
  case PTHREAD_CREATE_DETACHED:
    a->detached = 1;
    break;
  case PTHREAD_CREATE_JOINABLE:
    a->detached = 0;
    break;
  default:
    klee_warning("ignoring (EINVAL for bad detach for pthread_attr_setdetachstate)");
    return EINVAL;
  }
  return 0;
}
void   pthread_yield(void) {
  __thread_yield();
}

