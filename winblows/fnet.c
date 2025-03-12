/* started 2025.03.08 - MIT - https://github.com/skullchap/fnet */

#define nil 		((void*)0)

#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include <winsock2.h>
	#include <ws2tcpip.h>
	//#include <windows.h>
	#include <afunix.h>
	#include <fcntl.h>
	#include <io.h>
	#pragma comment(lib, "ws2_32.lib")

	typedef SOCKET sockt;
	#define close closesocket
#else
	#include <sys/socket.h>
	#include <sys/un.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <errno.h>

	typedef int sockt;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fnet.h"

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned long	ulong;

typedef struct	NetAddr NetAddr;
typedef struct 	ParsedV4V6 ParsedV4V6;
typedef enum	ConnType ConnType;
typedef enum	SockDomain SockDomain;
typedef enum	SockType SockType;

enum
{
	ProtoStrMaxLen = 8,
	AddrStrMaxLen = 120
};

enum ConnType
{
	Dial,
	Listen
};

enum SockDomain
{
	Local,
	Unix = Local,
	IPv4,
	IPv6
};

enum SockType
{
	Stream,
	DgRam,
};

struct ParsedV4V6
{
	char	ip[AddrStrMaxLen];
	ushort	port;
	int	isv6;
};

struct NetAddr
{
	char proto[ProtoStrMaxLen], addr[AddrStrMaxLen];
};

struct NetConn
{
	FILE* f;
	ConnType   conntype;
	SockDomain sockdomain;
	SockType   socktype;
	ParsedV4V6 v4v6;
	NetAddr    remote, local;
	sockt fd;
};

static NetConn*	newnetconn(char* proto, char* addr, ConnType ct);
static int	setsockdomaintype(NetConn* c, char* proto, char* addr);
static sockt	sockdial(NetConn* c);
static sockt	socklisten(NetConn* c);
static sockt	sockaccept(NetConn* c);
static int	setlocaddr(NetConn* c);
static int	setremaddr(NetConn* c);
static FILE*	fdfile(sockt sock);
static int	parsev4v6(char* addr, ParsedV4V6* result);
static int	setfneterr(char* fmt, ...);

static char* sockerrstr();
static char* errnostr();

static
int
setsockdomaintype(NetConn* c, char* proto, char* addr)
{
	int v4v6 = 0;
	ParsedV4V6 paddr;

	if (strncmp("tcp", proto, ProtoStrMaxLen) == 0) {
		v4v6 = 1;
		c->socktype = Stream;
	}
	else if (strncmp("udp", proto, ProtoStrMaxLen) == 0) {
		v4v6 = 1;
		c->socktype = DgRam;
	}
	else if (strncmp("unix", proto, ProtoStrMaxLen) == 0) {
		c->sockdomain = Unix;
		c->socktype = Stream;
	}
	else {
		setfneterr("unknown proto (%s)", proto);
		return -1;
	}
	if (v4v6) {
		if (parsev4v6(addr, &paddr) < 0)
			return -1;
		if (paddr.isv6)
			c->sockdomain = IPv6;
		else
			c->sockdomain = IPv4;
		c->v4v6 = paddr;
	}
	return 0;
}

static
NetConn*
newnetconn(char* proto, char* addr, ConnType ct)
{
	NetConn* c;

	c = calloc(1, sizeof(NetConn));
	if (!c)
		return nil;

	c->conntype = ct;
	if (setsockdomaintype(c, proto, addr) < 0) {
		free(c);
		return nil;
	}
	strncpy(c->local.proto, proto, ProtoStrMaxLen);
	strncpy(c->remote.proto, proto, ProtoStrMaxLen);
	if (ct == Dial)
		strncpy(c->remote.addr, addr, AddrStrMaxLen);
	else
		strncpy(c->local.addr, addr, AddrStrMaxLen);
	return c;
}

static
int
dialisten(NetConn* c)
{
	if (c->conntype == Dial) {
		if ((c->fd = sockdial(c)) < 0) {
			return -1;
		}
	}
	else {
		if ((c->fd = socklisten(c)) < 0) {
			return -1;
		}
	}
	if (!(c->f = fdfile(c->fd))) {
		return -1;
	}
	return 0;
}

NetConn*
fnetdial(char* proto, char* addr)
{
	NetConn* c = newnetconn(proto, addr, Dial);
	if (!c)
		return nil;
	if (dialisten(c) < 0) {
		fnetclose(c);
		return nil;
	}
	return c;
}

NetConn*
fnetlisten(char* proto, char* addr)
{
	NetConn* c = newnetconn(proto, addr, Listen);
	if (!c)
		return nil;
	if (dialisten(c) < 0) {
		fnetclose(c);
		return nil;
	}
	return c;
}

NetConn*
fnetaccept(NetConn* servc)
{
	NetConn* clientc;

	if (servc->socktype == DgRam) {
		setfneterr("fnetaccept should be applied on stream sockets");
		return nil;
	}

	clientc = newnetconn(servc->local.proto, servc->local.addr, Listen);
	if (!clientc)
		return nil;

	if ((clientc->fd = sockaccept(servc)) < 0) {
		fnetclose(clientc);
		return nil;
	}
	if (!(clientc->f = fdfile(clientc->fd))) {
		close(clientc->fd);
		fnetclose(clientc);
		return nil;
	}
	return clientc;
}

FILE* 	fnetf(NetConn* c) { return c->f; }
void	fnetclose(NetConn* c) { if (c->f)fclose(c->f); free(c); }

char*
fnetlocaddr(NetConn* c)
{
	if (c->conntype == Dial && c->local.addr[0] == '\0')
		setlocaddr(c);
	return c->local.addr;
}

char*
fnetremaddr(NetConn* c)
{
	if (c->conntype == Listen &&
		(c->socktype == DgRam || c->remote.addr[0] == '\0'))
		setremaddr(c);
	return c->remote.addr;
}


typedef struct sockaddr_in 	SockaddrIn;
typedef struct sockaddr_in6 	SockaddrIn6;
typedef struct sockaddr_un 	SockaddrUnix;
typedef struct sockaddr 	Sockaddr;
typedef struct sockaddr_storage	SockaddrStorage;

static
sockt
sockdial(NetConn* c)
{
	sockt fd;
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	SockaddrUnix addrUnix;
	uchar domain, type;
	void* paddr; uint addrlen;

	switch (c->sockdomain)
	{
	case Unix: domain = AF_UNIX; break;
	case IPv4: domain = AF_INET; break;
	case IPv6: domain = AF_INET6; break;
	default: setfneterr("bad socket domain (%d)", c->sockdomain); return -1;
	}
	switch (c->socktype)
	{
	case Stream: type = SOCK_STREAM; break;
	case DgRam:  type = SOCK_DGRAM; break;
	default: setfneterr("bad socket type (%d)", c->socktype); return -1;
	}

	fd = socket(domain, type, 0);
	if (fd < 0) {
		setfneterr("socket creation failed (%s)", sockerrstr());
		return -1;
	}

	switch (c->sockdomain)
	{
	case IPv4:
		memset(&addrV4, 0, sizeof(addrV4));
		addrV4.sin_family = AF_INET;
		addrV4.sin_port = htons(c->v4v6.port);
		addrV4.sin_addr.s_addr = inet_addr(c->v4v6.ip);
		paddr = &addrV4;
		addrlen = sizeof(addrV4);
		break;
	case IPv6:
		memset(&addrV6, 0, sizeof(addrV6));
		addrV6.sin6_family = AF_INET6;
		addrV6.sin6_port = htons(c->v4v6.port);
		inet_pton(AF_INET6, c->v4v6.ip, &addrV6.sin6_addr);
		paddr = &addrV6;
		addrlen = sizeof(addrV6);
		break;
	case Unix:
		memset(&addrUnix, 0, sizeof(addrUnix));
		addrUnix.sun_family = AF_UNIX;
		strncpy(addrUnix.sun_path, c->remote.addr, sizeof(addrUnix.sun_path) - 1);
		paddr = &addrUnix;
		addrlen = sizeof(addrUnix);
		break;
	default:
		close(fd);
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if (connect(fd, paddr, addrlen) < 0) {
		close(fd);
		setfneterr("connection failed (%s)", sockerrstr());
		return -1;
	}
	return fd;
}

static
sockt
socklisten(NetConn* c)
{
	sockt fd;
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	SockaddrUnix addrUnix;
	uchar domain, type;
	void* paddr; uint addrlen;

	switch (c->sockdomain)
	{
	case Unix: domain = AF_UNIX; break;
	case IPv4: domain = AF_INET; break;
	case IPv6: domain = AF_INET6; break;
	default: setfneterr("bad socket domain (%d)", c->sockdomain); return -1;
	}
	switch (c->socktype)
	{
	case Stream: type = SOCK_STREAM; break;
	case DgRam:  type = SOCK_DGRAM; break;
	default: setfneterr("bad socket type (%d)", c->socktype); return -1;
	}

	fd = socket(domain, type, 0);
	if (fd < 0) {
		setfneterr("socket creation failed (%s)", sockerrstr());
		return -1;
	}

	switch (c->sockdomain)
	{
	case IPv4:
		memset(&addrV4, 0, sizeof(addrV4));
		addrV4.sin_family = AF_INET;
		addrV4.sin_port = htons(c->v4v6.port);
		addrV4.sin_addr.s_addr = inet_addr(c->v4v6.ip);
		paddr = &addrV4;
		addrlen = sizeof(addrV4);
		break;
	case IPv6:
		memset(&addrV6, 0, sizeof(addrV6));
		addrV6.sin6_family = AF_INET6;
		addrV6.sin6_port = htons(c->v4v6.port);
		inet_pton(AF_INET6, c->v4v6.ip, &addrV6.sin6_addr);
		paddr = &addrV6;
		addrlen = sizeof(addrV6);
		break;
	case Unix:
		memset(&addrUnix, 0, sizeof(addrUnix));
		addrUnix.sun_family = AF_UNIX;
		strncpy(addrUnix.sun_path, c->remote.addr, sizeof(addrUnix.sun_path) - 1);
		paddr = &addrUnix;
		addrlen = sizeof(addrUnix);
		break;
	default:
		close(fd);
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if (bind(fd, paddr, addrlen) < 0) {
		close(fd);
		setfneterr("bind failed (%s)", sockerrstr());
		return -1;
	}

	if (c->socktype == Stream) {
		if (listen(fd, 128) < 0) {
			close(fd);
			setfneterr("listen failed (%s)", sockerrstr());
			return -1;
		}
	}
	return fd;
}

sockt
sockaccept(NetConn* c)
{
	sockt cfd = accept(c->fd, nil, nil);
	if (cfd < 0) {
		setfneterr("accept failed (%s)", sockerrstr());
		return -1;
	}
	return cfd;
}

static
int
setlocaddr(NetConn* c)
{
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	void* paddr; uint addrlen;
	char buf[AddrStrMaxLen];

	switch (c->sockdomain)
	{
	case IPv4:
		paddr = &addrV4;
		addrlen = sizeof(addrV4);
		break;
	case IPv6:
		paddr = &addrV6;
		addrlen = sizeof(addrV6);
		break;
	case Unix:
		strncpy(c->local.addr, c->remote.addr, AddrStrMaxLen);
		return 0;
	default:
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if (getsockname(c->fd, paddr, &addrlen) < 0) {
		setfneterr("getsockname failed (%s)", sockerrstr());
		return -1;
	}
	if (c->sockdomain == IPv4) {
		if (inet_ntop(AF_INET, &addrV4.sin_addr, buf, sizeof(buf)) == nil) {
			setfneterr("inet_ntop failed (%s)", sockerrstr());
			return -1;
		}
		snprintf(c->local.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV4.sin_port));
	}
	if (c->sockdomain == IPv6) {
		if (inet_ntop(AF_INET6, &addrV6.sin6_addr, buf, sizeof(buf)) == nil) {
			setfneterr("inet_ntop failed (%s)", sockerrstr());
			return -1;
		}
		snprintf(c->local.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV6.sin6_port));
	}
	return 0;
}

static
int
setremaddr(NetConn* c)
{
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	void* paddr; uint addrlen;
	char buf[AddrStrMaxLen];

	switch (c->sockdomain)
	{
	case IPv4:
		paddr = &addrV4;
		addrlen = sizeof(addrV4);
		break;
	case IPv6:
		paddr = &addrV6;
		addrlen = sizeof(addrV6);
		break;
	case Unix:
		strncpy(c->remote.addr, c->local.addr, AddrStrMaxLen);
		return 0;
	default:
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if (c->socktype == Stream) {
		if (getpeername(c->fd, paddr, &addrlen) < 0) {
			setfneterr("getpeername failed (%s)", sockerrstr());
			return -1;
		}
	}
	else {
		char b[1];
		if (recvfrom(c->fd, b, 1, MSG_PEEK, paddr, &addrlen) < 0) {
			setfneterr("recvfrom failed (%s)", sockerrstr());
			return -1;
		}
	}

	if (c->sockdomain == IPv4) {
		if (inet_ntop(AF_INET, &addrV4.sin_addr, buf, sizeof(buf)) == nil) {
			setfneterr("inet_ntop failed (%s)", sockerrstr());
			return -1;
		}
		snprintf(c->remote.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV4.sin_port));
	}
	if (c->sockdomain == IPv6) {
		if (inet_ntop(AF_INET6, &addrV6.sin6_addr, buf, sizeof(buf)) == nil) {
			setfneterr("inet_ntop failed (%s)", sockerrstr());
			return -1;
		}
		snprintf(c->remote.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV6.sin6_port));
	}
	return 0;
}

#ifdef _WIN32
static 
FILE *
fdfile(sockt sock)
{
	int fd;
	FILE *f;
	
	fd = _open_osfhandle((intptr_t)sock, _O_RDWR);
	if(fd == -1){
		setfneterr("open_osfhandle failed (%s)", errnostr());
		return nil;
	}

	f = _fdopen(fd, "r+");
	if(f == nil){
		setfneterr("fdopen failed (%s)", errnostr());
		_close(fd);
		return nil;
	}

	return f;
}
#else
static
FILE *
fdfile(int fd)
{
	FILE* f = fdopen(fd, "r+");
	if(f == nil){
		setfneterr("fdopen failed (%s)", errnostr());
		return nil;
	}
	return f;
}
#endif


static
int
validate_ipv4(char* ip_str)
{
	SockaddrIn sa;
	return inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1;
}

static
int
validate_ipv6(char* ip_str)
{
	SockaddrIn6 sa6;
	return inet_pton(AF_INET6, ip_str, &(sa6.sin6_addr)) == 1;
}

static
int
parsev4v6(char* addr, ParsedV4V6* result)
{
	char ip[AddrStrMaxLen];
	int port = 0;
	char* colon_pos;
	char* port_part;

	if (addr[0] == '[') {
		char *rbrak = strchr(addr, ']');
		if (!rbrak) {
			setfneterr("invalid IPv6 format or missing port");
			return -1;
		}
		colon_pos = strchr(rbrak, ':');
		if (!colon_pos) {
			setfneterr("missing port for IPv6");
			return -1;
		}
		strncpy(ip, addr + 1, rbrak - addr - 1);
		ip[rbrak - addr - 1] = '\0';
		port_part = colon_pos + 1;
	}
	else {
		colon_pos = strrchr(addr, ':');
		if (!colon_pos) {
			setfneterr("missing port for IP address");
			return -1;
		}
		strncpy(ip, addr, colon_pos - addr);
		ip[colon_pos - addr] = '\0';
		port_part = colon_pos + 1;
	}

	if (validate_ipv4(ip)) {
		result->isv6 = 0;
	}
	else if (validate_ipv6(ip)) {
		result->isv6 = 1;
	}
	else {
		setfneterr("invalid IP address");
		return -1;
	}

	if (sscanf(port_part, "%d", &port) != 1 || port < 1 || port > 65535) {
		setfneterr("invalid port");
		return -1;
	}

	strncpy(result->ip, ip, AddrStrMaxLen);
	result->port = port;
	return 0;
}



/* error.c */

#include <errno.h>
#include <stdarg.h>

# if __STDC_VERSION__ <= 202311L  /* in C23 thread_local is a builtin keyword */
/* https://stackoverflow.com/a/18298965 */
# ifndef thread_local
# if __STDC_VERSION__ >= 201112L && !defined __STDC_NO_THREADS__
#  define thread_local _Thread_local
# elif defined _WIN32 && ( \
	   defined _MSC_VER || \
	   defined __ICL || \
	   defined __DMC__ || \
	   defined __BORLANDC__ )
#  define thread_local __declspec(thread) 
/* note that ICC (linux) and Clang are covered by __GNUC__ */
# elif defined __GNUC__ || \
	   defined __SUNPRO_C || \
	   defined __xlC__
#  define thread_local __thread
# else
# warning "Cannot define thread_local"
# define thread_local
# define no_thread_local
# endif
# endif
# endif

static	thread_local char estr[256];

char* fneterr(void) { return estr; }
char* errnostr(void) { return strerror(errno); }

char*
sockerrstr()
{
#ifdef _WIN32
	int err;
	static thread_local char msgbuf[256];

	msgbuf[0] = '\0';
	err = WSAGetLastError();
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nil, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		msgbuf, sizeof(msgbuf), nil);

	size_t len = strlen(msgbuf);
	while (len > 0 && (msgbuf[len - 1] == '\r' || msgbuf[len - 1] == '\n')) {
		msgbuf[--len] = '\0';
	}

	return msgbuf;
#else
	return strerror(errno);
#endif
}


int
setfneterr(char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (vsnprintf(estr, sizeof(estr), fmt, args) < 0) {
		static char emsg[] = "setfneterr(): vsnprintf error";
		memcpy(estr, emsg, sizeof(emsg));
	}
	va_end(args);
	return 0;
}

int
fnetinitlib(void)
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		setfneterr("WSAStartup failed (%s)", sockerrstr());
		return -1;
	}
	return 0;
#endif
}


int
fnetcleanlib(void)
{
#ifdef _WIN32
	return WSACleanup();
#endif
}
