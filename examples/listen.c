#include <stdio.h>
#include <string.h>
#include "fnet.h"

int fork(void);

int
main(int argc, char **argv)
{
	NetConn *serv, *client;
	char buf[256];
	int isudp= 0;

	if(argc < 3){
		printf("forgot to pass proto or address? (e.g. \"%s tcp 127.0.0.1:9999\")\n", argv[0]);
		return -1;
	}

	if(strstr(argv[1], "udp")){
		isudp = 1;
	}

	serv = fnetlisten(argv[1], argv[2]);
	if(!serv){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	while(1){
		if(!isudp){
			client = fnetaccept(serv);
			if(!client){
				fprintf(stderr, "fnetaccept: %s\n", fneterr());
				continue;
			}
			printf("Connected (%s <- %s)\n", fnetlocaddr(client), fnetremaddr(client));
		}
		if(!isudp && fork()){
			fnetclose(client);
		}else{
			if(isudp)
				client = serv;
			while(1){
				if(isudp)
					printf("New Msg (%s <- %s)\n", fnetlocaddr(client), fnetremaddr(client));
				if(!fgets(buf, sizeof(buf), fnetf(client)))
					break;
				fputs(buf, stdout);
				fflush(stdout);
			}
			printf("Closed (%s <- %s)\n", fnetlocaddr(client), fnetremaddr(client));
			fnetclose(client);
			return 0;
		}
	}
	fnetclose(serv);
	return 0;
}