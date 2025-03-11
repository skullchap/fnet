/* started 2025.03.08 - MIT - https://github.com/skullchap/fnet */

typedef struct	NetConn NetConn;

NetConn*	fnetdial(char *proto, char* addr);
NetConn*	fnetlisten(char *proto, char* addr);
NetConn*	fnetaccept(NetConn*);
char*		fneterr(void);
FILE*		fnetf(NetConn*);
char*		fnetlocaddr(NetConn*);
char*		fnetremaddr(NetConn*);
void 		fnetclose(NetConn*);
