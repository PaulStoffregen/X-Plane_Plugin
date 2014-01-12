#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>


void die(const char *format, ...);

#define PORTNUM 29372


int main(int argc, char **argv)
{
	int i, serv, client;
	struct sockaddr_in addr;
	socklen_t len;
	char buf[64];

	serv = socket(AF_INET, SOCK_STREAM, 0);
	if (serv < 0) die("unable to get network socket\n");
	i = 1;
	setsockopt(serv, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORTNUM);
	i = bind(serv, (struct sockaddr *)&addr, sizeof(addr));
	if (i < 0) die("unable to bind to port %d\n", PORTNUM);
	i = listen(serv, 5);
	if (i < 0) die("unable to listen on port %d\n", PORTNUM);

	while (1) {
		printf("listening...\n");
		len = sizeof(addr);
		client = accept(serv, (struct sockaddr *)&addr, &len);
		if (client < 0) die("error while accepting incoming connection\n");
		printf("client connected %s:%d\n",
		 	inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)
		);
		while (1) {
			i = recv(client, buf, sizeof(buf)-1, 0);
			//printf("i = %d\n", i);
			if (i < 1) break;
			if (i > 0 && i < sizeof(buf)) {
				buf[i] = 0;
				printf("%s", buf);
			}
		}
		fflush(stdout);
		printf("client seems to have disconnected...\n");
		shutdown(client, SHUT_RDWR);
		close(client);
	}
	return 0;
}



void die(const char *format, ...)
{
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        exit(1);
}

