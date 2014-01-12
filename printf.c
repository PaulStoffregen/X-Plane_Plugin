#include "TeensyControls.h"

#ifdef USE_PRINTF_DEBUG

//#define USE_LOCK

static int sock = -1;
static int sock_connected = 0;
static int sock_connecting = 0;

//static int printf_count = 0;
//static int sock_created_count = 0;
//static int sock_connect_count = 0;
//static int sock_send_count = 0;
//static int sock_inprogress_count = 0;
//static int sock_connect_wait_count = 0;

#ifdef USE_LOCK
static pthread_mutex_t printf_mutex;
#endif

void TeensyControls_printf_init(void)
{
	#ifdef USE_LOCK
	pthread_mutex_init(&printf_mutex, NULL);
	#endif
	printf("\n");
}

static int nonblock(int s)
{
#if defined(WINDOWS) || defined(WINDOWS64)
	unsigned long mode=1;
	ioctlsocket(s, FIONBIO, &mode); 
	return 1;
#else
	int i;
	i = fcntl(sock, F_GETFL);
	if (fcntl(sock, F_SETFL, i | O_NONBLOCK) < 0) return 0;
	return 1;
#endif
}

static int connect_in_progress(void)
{
#if defined(WINDOWS) || defined(WINDOWS64)
	return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
	return (errno == EINPROGRESS);
#endif
}

#if defined(WINDOWS) || defined(WINDOWS64)
#define socklen_t int
#endif


int TeensyControls_printf(const char *format, ...)
{
	char buf[4096];
	struct sockaddr_in addr;
	struct timeval tv;
	fd_set fds;
	socklen_t slen;
	int i, len=0;

	#ifdef USE_LOCK
	pthread_mutex_lock(&printf_mutex);
	#endif
	va_list args;
	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	if (len <= 0) goto ok;
	//printf_count++;
	if (sock < 0) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0) goto fail;
		if (!nonblock(sock)) goto fail;
		//i = fcntl(sock, F_GETFL);
		//if (fcntl(sock, F_SETFL, i | O_NONBLOCK) < 0) goto fail;
		sock_connected = 0;
		sock_connecting = 0;
		//sock_created_count++;
	}
	if (!sock_connected) {
		if (!sock_connecting) {
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(PRINTF_PORT);
			addr.sin_addr.s_addr = inet_addr(PRINTF_ADDR);
			//i = inet_aton(PRINTF_ADDR, &(addr.sin_addr));
			//if (i == -1) goto fail;
			i = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
			if (i == 0) {
				sock_connecting = 0;
				sock_connected = 1;
				//sock_connect_count++;
			} else if (connect_in_progress()) {
				sock_connecting = 1;
				sock_connected = 0;
				//sock_inprogress_count++;
				goto ok;
			} else {
				goto fail;
			}
		} else {
			// The socket is non-blocking and the connection cannot be completed
			// immediately.  It is possible to select(2) for completion by
			// selecting the socket for writing.
			FD_ZERO(&fds);
			FD_SET(sock, &fds);
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			i = select(sock+1, NULL, &fds, NULL, &tv);
			if (i > 0) {
				// After select(2) indicates writability, use getsockopt(2)
				// to read the SO_ERROR option at level SOL_SOCKET to
				// determine whether connect() completed successfully
				// (SO_ERROR is zero) or unsuccessfully
				slen = sizeof(i);
				if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &i, &slen) || i != 0) {
					goto fail;
				}
				sock_connecting = 0;
				sock_connected = 1;
				//sock_connect_count++;
			} else if (i < 0) goto fail;
			//sock_connect_wait_count++;
		}
	}
	if (sock_connected) {
		i = send(sock, buf, len, 0);
		//sock_send_count++;
		if (i < 0 && errno != EINTR && errno != EAGAIN) goto fail;
	}
ok:
	#ifdef USE_LOCK
	pthread_mutex_unlock(&printf_mutex);
	#endif
	return len;
fail:
	if (sock >= 0) close(sock);
	sock = -1;
	sock_connected = 0;
	sock_connecting = 0;
	#ifdef USE_LOCK
	pthread_mutex_unlock(&printf_mutex);
	#endif
	return len;
}

#endif // USE_PRINTF_DEBUG
