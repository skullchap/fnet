/* started 2025.03.08 - MIT - https://github.com/skullchap/fnet */

#include "defs.h"
#include "fnet.h"

typedef struct	NetAddr NetAddr;
typedef struct 	ParsedV4V6 ParsedV4V6;
typedef enum	ConnType ConnType;
typedef enum	SockDomain SockDomain;
typedef enum	SockType SockType;

enum
{
	ProtoStrMaxLen=	8,
	AddrStrMaxLen=	120
};

enum ConnType
{
	Dial,
	Listen
};

enum SockDomain
{
	Local,
	Unix=	Local,
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
	FILE *f;
	ConnType   conntype;
	SockDomain sockdomain;
	SockType   socktype;
	ParsedV4V6 v4v6;
	NetAddr    remote, local;
	int fd;
};

int		close(int fd);
FILE*		fdopen(int fd, const char *mode);

static NetConn*	newnetconn(char *proto, char* addr, ConnType ct);
static int	setsockdomaintype(NetConn *c, char *proto, char *addr);
static int	sockdial(NetConn *c);
static int	socklisten(NetConn *c);
static int	sockaccept(NetConn *c);
static int	setlocaddr(NetConn *c);
static int	setremaddr(NetConn *c);
static FILE*	fdfile(int fd);
static int	parsev4v6(char *addr, ParsedV4V6 *result);

static
int
setsockdomaintype(NetConn *c, char *proto, char *addr)
{
	int r, v4v6= 0;
	ParsedV4V6 paddr;

	if(strncmp("tcp", proto, ProtoStrMaxLen) == 0){
		v4v6=1;
		c->socktype= Stream;
	}else if(strncmp("udp", proto, ProtoStrMaxLen) == 0){
		v4v6=1;
		c->socktype= DgRam;
	}else if(strncmp("unix", proto, ProtoStrMaxLen) == 0){
		c->sockdomain=	Unix;
		c->socktype=	Stream;
	}else{
		setfneterr("unknown proto (%s)", proto);
		return -1;
	}
	if(v4v6){
		if(parsev4v6(addr, &paddr) < 0)
			return -1;
		if(paddr.isv6)
			c->sockdomain = IPv6;
		else
			c->sockdomain = IPv4;
		c->v4v6 = paddr;
	}
	return 0;
}

static
NetConn*
newnetconn(char *proto, char* addr, ConnType ct)
{
	NetConn *c;
	int fd;
	FILE *f;

	c = calloc(1, sizeof(NetConn));
	if(!c)
		return nil;
	
	c->conntype = ct;
	if(setsockdomaintype(c, proto, addr) < 0){
		free(c);
		return nil;
	}
	strncpy(c->local.proto, proto, ProtoStrMaxLen);
	strncpy(c->remote.proto, proto, ProtoStrMaxLen);
	if(ct == Dial)
		strncpy(c->remote.addr, addr, AddrStrMaxLen);
	else
		strncpy(c->local.addr, addr, AddrStrMaxLen);
	return c;
}

static
int
dialisten(NetConn *c)
{
	if(c->conntype == Dial){
		if((c->fd = sockdial(c)) < 0){
			return -1;
		}
	}else{
		if((c->fd = socklisten(c)) < 0){
			return -1;
		}
	}
	if(!(c->f = fdfile(c->fd))){
		return -1;
	}
	return 0;	
}

NetConn *
fnetdial(char *proto, char* addr)
{
	NetConn *c = newnetconn(proto, addr, Dial);
	if(!c)
		return nil;
	if(dialisten(c) < 0){
		fnetclose(c);
		return nil;
	}
	return c;
}

NetConn *
fnetlisten(char *proto, char* addr)
{
	NetConn *c = newnetconn(proto, addr, Listen);
	if(!c)
		return nil;
	if(dialisten(c) < 0){
		fnetclose(c);
		return nil;
	}
	return c;
}

NetConn *
fnetaccept(NetConn *servc)
{
	NetConn	*clientc;

	if(servc->socktype == DgRam){
		setfneterr("fnetaccept should be applied on stream sockets");
		return nil;
	}

	clientc = newnetconn(servc->local.proto, servc->local.addr, Listen);
	if(!clientc)
		return nil;
		
	if((clientc->fd = sockaccept(servc)) < 0){
		fnetclose(clientc);
		return nil;
	}
	if(!(clientc->f = fdfile(clientc->fd))){
		close(clientc->fd);
		fnetclose(clientc);
		return nil;
	}
	return clientc;
}

FILE*	fnetf(NetConn *c)	{return c->f;}
void	fnetclose(NetConn *c)	{fclose(c->f); free(c);}

char*
fnetlocaddr(NetConn *c)
{
	if(c->conntype == Dial && c->local.addr[0] == '\0')
		setlocaddr(c);
	return c->local.addr;
}

char*
fnetremaddr(NetConn *c)
{
	if(c->conntype == Listen && 
	(c->socktype == DgRam || c->remote.addr[0] == '\0'))
		setremaddr(c);
	return c->remote.addr;
}

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct sockaddr_in 	SockaddrIn;
typedef struct sockaddr_in6 	SockaddrIn6;
typedef struct sockaddr_un 	SockaddrUnix;
typedef struct sockaddr 	Sockaddr;
typedef struct sockaddr_storage	SockaddrStorage;

static
int
sockdial(NetConn *c)
{
	int fd;
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	SockaddrUnix addrUnix;
	uchar domain, type;
	void *paddr; uint addrlen;

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
	if(fd < 0){
		setfneterr("socket creation failed (%s)", errnostr());
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
    		strncpy(addrUnix.sun_path, c->remote.addr, sizeof(addrUnix.sun_path)-1);
		paddr = &addrUnix;
		addrlen = sizeof(addrUnix);
		break;
	default:
		close(fd);
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if(connect(fd, paddr, addrlen) < 0){
		close(fd);
		setfneterr("connection failed (%s)", errnostr());
		return -1;
	}
	return fd;
}

static
int
socklisten(NetConn *c)
{
	int fd;
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	SockaddrUnix addrUnix;
	uchar domain, type;
	void *paddr; uint addrlen;

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
	if(fd < 0){
		setfneterr("socket creation failed (%s)", errnostr());
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
    		strncpy(addrUnix.sun_path, c->remote.addr, sizeof(addrUnix.sun_path)-1);
		paddr = &addrUnix;
		addrlen = sizeof(addrUnix);
		break;
	default:
		close(fd);
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if(bind(fd, paddr, addrlen) < 0){
		close(fd);
		setfneterr("bind failed (%s)", errnostr());
		return -1;
	}

	if(c->socktype == Stream){
		if(listen(fd, 128) < 0){
			close(fd);
			setfneterr("listen failed (%s)", errnostr());
			return -1;
		}
	}
	return fd;
}

int
sockaccept(NetConn *c)
{
	int cfd = accept(c->fd, nil, nil);
	if(cfd < 0){
		setfneterr("accept failed (%s)", errnostr());
		return -1;
	}
	return cfd;
}

static
int
setlocaddr(NetConn *c)
{
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	void *paddr; uint addrlen;
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
		break;
	default:
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if(getsockname(c->fd, paddr, &addrlen) < 0){
		setfneterr("getsockname failed (%s)", errnostr());
		return -1;
	}
	if(c->sockdomain == IPv4){
		if(inet_ntop(AF_INET, &addrV4.sin_addr, buf, sizeof(buf)) == nil){
			setfneterr("inet_ntop failed (%s)", errnostr());
			return -1;
		}
		snprintf(c->local.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV4.sin_port));
	}
	if(c->sockdomain == IPv6){
		if(inet_ntop(AF_INET6, &addrV6.sin6_addr, buf, sizeof(buf)) == nil){
			setfneterr("inet_ntop failed (%s)", errnostr());
			return -1;
		}
		snprintf(c->local.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV6.sin6_port));
	}
	return 0;
}

static
int
setremaddr(NetConn *c)
{
	SockaddrIn addrV4;
	SockaddrIn6 addrV6;
	void *paddr; uint addrlen;
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
		break;
	default:
		setfneterr("bad domain (%d)", c->sockdomain);
		return -1;
	}

	if(c->socktype == Stream){
		if(getpeername(c->fd, paddr, &addrlen) < 0){
			setfneterr("getpeername failed (%s)", errnostr());
			return -1;
		}
	}else{
		char b[1];
		if(recvfrom(c->fd, b, 1, MSG_PEEK, paddr, &addrlen) < 0){
			setfneterr("recvfrom failed (%s)", errnostr());
			return -1;
		}
	}

	if(c->sockdomain == IPv4){
		if(inet_ntop(AF_INET, &addrV4.sin_addr, buf, sizeof(buf)) == nil){
			setfneterr("inet_ntop failed (%s)", errnostr());
			return -1;
		}
		snprintf(c->remote.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV4.sin_port));
	}
	if(c->sockdomain == IPv6){
		if(inet_ntop(AF_INET6, &addrV6.sin6_addr, buf, sizeof(buf)) == nil){
			setfneterr("inet_ntop failed (%s)", errnostr());
			return -1;
		}
		snprintf(c->remote.addr, AddrStrMaxLen, "%s:%d", buf, ntohs(addrV6.sin6_port));
	}
	return 0;
}

static
FILE*
fdfile(int fd)
{
	FILE *sf;

	sf = fdopen(fd, "r+");
   	if(sf == nil){
		setfneterr("fdopen failed (%s)", errnostr());
		return nil;
	}
	/* setvbuf(sf, nil, IONBF, 0); */
	return sf;
}

static
int
validate_ipv4(char *ip_str)
{
	SockaddrIn sa;
	return inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1;
}

static
int
validate_ipv6(char *ip_str)
{
	SockaddrIn6 sa6;
	return inet_pton(AF_INET6, ip_str, &(sa6.sin6_addr)) == 1;
}

static
int
parsev4v6(char *addr, ParsedV4V6 *result)
{
	char ip[AddrStrMaxLen];
	int port= 0;
	char *colon_pos;
	char *port_part;

	if(addr[0] == '['){
		char *rbrak = strchr(addr, ']');
		if(!rbrak){
			setfneterr("invalid IPv6 format or missing port");
			return -1;
		}
		colon_pos = strchr(rbrak, ':');
		if(!colon_pos){
			setfneterr("missing port for IPv6");
			return -1;
		}
		strncpy(ip, addr + 1, rbrak - addr - 1);
		ip[rbrak - addr - 1] = '\0';
		port_part = colon_pos + 1;
	}else{
		colon_pos = strrchr(addr, ':');
		if(!colon_pos){
			setfneterr("missing port for IP address");
			return -1;
		}
		strncpy(ip, addr, colon_pos - addr);
		ip[colon_pos - addr] = '\0';
		port_part = colon_pos + 1;
	}

	if(validate_ipv4(ip)){
		result->isv6 = 0;
	}else if(validate_ipv6(ip)){
		result->isv6 = 1;
	}else{
		setfneterr("invalid IP address");
		return -1;
	}

	if(sscanf(port_part, "%d", &port) != 1 || port < 1 || port > 65535){
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

static thread_local char estr[256];

int	vsnprintf(char *, ulong, const char *, va_list);

char*	fneterr(void)	{return estr;}
char*	errnostr(void)	{return strerror(errno);}

int
setfneterr(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if(vsnprintf(estr, sizeof(estr), fmt, args) < 0){
		static char emsg[] = "setfneterr(): vsnprintf error";
		memcpy(estr, emsg, sizeof(emsg));
	}
	va_end(args);
	return 0;
}
