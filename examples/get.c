#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "fnet.h"

int resolvhost(char *hostname, char *buf, size_t n);

int
main(int argc, char **argv)
{
	NetConn *c;
	char ip[128], buf[256<<10], *body;

	if (argc < 2){
		printf("forgot to pass link? (e.g. \"%s example.com\")\n", argv[0]);
		return -1;
	}

	if(resolvhost(argv[1], ip, sizeof(ip)) < 0) {
		fprintf(stderr, "resolvhost: can't resolve host\n");
		return -1;
    	} 

	c = fnetdial("tcp", strcat(ip, ":80"));
	if(!c){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	fprintf(fnetf(c),
		"GET / HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n\r\n", argv[1]);

	fread(buf, sizeof(buf), 1, fnetf(c));

	body = strstr(buf, "\r\n\r\n");
	if(body){
		body += 4;  /* move past "\r\n\r\n" */
		fwrite(buf, 1, body - buf, stderr); /* headers */
		fputs(body, stdout);
	}
	fnetclose(c);
	return 0;
}

int 
resolvhost(char *hostname, char *buf, size_t n)
{
	struct addrinfo hints, *res;
	void *addr;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if(getaddrinfo(hostname, NULL, &hints, &res) != 0){
		return -1;
	}

	if(res->ai_family == AF_INET){
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
		addr = &(ipv4->sin_addr);
	}else{
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
		addr = &(ipv6->sin6_addr);
	}

	if(inet_ntop(res->ai_family, addr, buf, n) == NULL){
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);
	return 0;
}
