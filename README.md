# fnet - FILE* over your socks.

#### Create, send and receive data over your tcp, udp and unix sockets with familiar C standard library FILE* IO interface.

```c
NetConn *c = fnetdial("tcp", "127.0.0.1:9999");
fprintf(fnetf(c), "hello!\n");
```

#### Dial/Listen should be familiar to Plan9'ers and Gophers.
```c
NetConn*	fnetdial(char *proto, char *addr);
NetConn*	fnetlisten(char *proto, char *addr);
NetConn*	fnetaccept(NetConn*);
char*		fneterr(void);
FILE*		fnetf(NetConn*);
char*		fnetlocaddr(NetConn*);
char*		fnetremaddr(NetConn*);
void		fnetclose(NetConn*);
```
#### Check examples of dialing and listening.
```bash
cd examples/
./compile dial && ./compile listen
# ./listen tcp 127.0.0.1:9999 # ./dial tcp 127.0.0.1:9999
```

## License
MIT
