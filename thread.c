#include "TeensyControls.h"


//http://www.cs.wustl.edu/~schmidt/win32-cv-1.html

#if defined(MACOSX) || defined(LINUX)

typedef struct {
	void (*function)(void*);
	void *arg;
} threadin_t;

static void * launcher(void *in)
{
	void (*function)(void*);
	void *arg;

	function = ((threadin_t *)in)->function;
	arg = ((threadin_t *)in)->arg;
	free(in);
	function(arg);
	pthread_exit(NULL);
	return NULL;
}

int thread_start(void (*function)(void*), void *arg)
{
	pthread_t pth;
	int r, retry;
	threadin_t *in;

	in = (threadin_t *)malloc(sizeof(threadin_t));
	if (!in) return 0;
	in->function = function;
	in->arg = arg;

	for (retry=0; retry<5; retry++) {
		r = pthread_create(&pth, NULL, launcher, in);
		if (r == 0) {
			pthread_detach(pth);
			return 1;
		}
		usleep(1000);
	}
	return 0;
}



#elif defined(WINDOWS) || defined(WINDOWS64)


typedef struct {
	void (*function)(void*);
	void *arg;
} threadin_t;

WINAPI DWORD launcher(void *in)
{
	void (*function)(void*);
	void *arg;

	function = ((threadin_t *)in)->function;
	arg = ((threadin_t *)in)->arg;
	free(in);
	function(arg);
	//pthread_exit(NULL); // TODO: windows thread exit function?
	return 0;
}

int thread_start(void (*function)(void*), void *arg)
{
	HANDLE th;
	threadin_t *in;

	in = (threadin_t *)malloc(sizeof(threadin_t));
	if (!in) return 0;
	in->function = function;
	in->arg = arg;

	// TODO: should we use _beginthreadex instead?
	th = CreateThread(NULL, 0, launcher, in, 0, NULL);
	if (th == NULL) return 0;
	return 1;
}

// From  http://www.cs.wustl.edu/~schmidt/win32-cv-1.html

// pthread broadcast feature removed - only signal is possible

// "_timeout" not quite POSIX...
int pthread_cond_wait_timeout(pthread_cond_t *cv, pthread_mutex_t *external_mutex, DWORD msec)
{
	DWORD result;
	EnterCriticalSection (&cv->lock);
	cv->count++;
	LeaveCriticalSection (&cv->lock);
	LeaveCriticalSection (external_mutex);
	result = WaitForSingleObject(cv->event, msec);
	EnterCriticalSection(&cv->lock);
	cv->count--;
	LeaveCriticalSection(&cv->lock);
	EnterCriticalSection(external_mutex);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cv)
{
	int have_waiters=0;

	EnterCriticalSection (&cv->lock);
	if (cv->count > 0) have_waiters = 1;
	LeaveCriticalSection (&cv->lock);
	if (have_waiters) SetEvent(cv->event);
	return 0;
}

int pthread_cond_init(pthread_cond_t *cv, const void *attr)
{
	cv->count = 0;
	InitializeCriticalSection(&cv->lock);
	cv->event = CreateEvent (NULL,  // no security
				FALSE, // auto-reset event
				FALSE, // non-signaled initially
				NULL); // unnamed
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	DeleteCriticalSection(&cond->lock);
	CloseHandle(cond->event);
	return 0;
}


#endif

