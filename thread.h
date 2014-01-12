#ifndef THREAD_WIN32_STUFF_
#define THREAD_WIN32_STUFF_
#if defined(WINDOWS) || defined(WINDOWS64)

typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_init(m, p) InitializeCriticalSection(m)
#define pthread_mutex_lock(m) EnterCriticalSection(m)
#define pthread_mutex_unlock(m) LeaveCriticalSection(m)
#define pthread_mutex_destroy(m) DeleteCriticalSection(m)

#define PTH_SIGNAL	0
#define PTH_BROADCAST	1
#define PTH_MAX_EVENTS	2

typedef struct {
	unsigned int count;
	CRITICAL_SECTION lock;
	HANDLE event;
} pthread_cond_t;


int pthread_cond_init(pthread_cond_t *cond, const void *attr);
int pthread_cond_wait_timeout(pthread_cond_t *cv, pthread_mutex_t *external_mutex, DWORD msec);
int pthread_cond_signal(pthread_cond_t *cv);
int pthread_cond_destroy(pthread_cond_t *cond);



#endif
#endif
