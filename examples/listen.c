#include <stdio.h>
#include <string.h>
#include "fnet.h"

int fork(void);
int stream(char **argv);
int dgram(char **argv);

int
main(int argc, char **argv)
{
	if(argc < 3){
		printf("forgot to pass proto or address? (e.g. \"%s tcp 127.0.0.1:9999\")\n", argv[0]);
		return -1;
	}
	if(strstr(argv[1], "udp")){
		return dgram(argv);
	}else{
		return stream(argv);
	}
	return 0;
}

int
stream(char **argv)
{
	NetConn *serv, *client;
	char buf[256];
	int n;

	serv = fnetlisten(argv[1], argv[2]);
	if(!serv){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	for(;;){
		client = fnetaccept(serv);
		if(!client){
			fprintf(stderr, "fnetaccept: %s\n", fneterr());
			continue;
		}
		printf("Connected (%s <- %s)\n", fnetlocaddr(client), fnetremaddr(client));
		if(fork()){
			fnetclose(client);
		}else{
			for(;;){
				n = snprintf(buf, sizeof(buf), "%s: ", fnetremaddr(client));
				if (!fgets(buf + n, sizeof(buf) - n, fnetf(client)))
					break;
				fputs(buf, stdout);

				fprintf(fnetf(client), "hello %s!\n", fnetremaddr(client));
			}
			printf("Closed (%s <- %s)\n", fnetlocaddr(client), fnetremaddr(client));
			return 0;
		}
	}
	fnetclose(serv);
	return 0;
}

int
dgram(char **argv)
{
	NetConn *in;
	char buf[256];
	int n;

	in = fnetlisten(argv[1], argv[2]);
	if(!in){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	for(;;){
		fnetaccept(in);
		printf("New Msg (%s <- %s)\n", fnetlocaddr(in), fnetremaddr(in));

		n = snprintf(buf, sizeof(buf), "%s: ", fnetremaddr(in));
		if(!fgets(buf + n, sizeof(buf) - n, fnetf(in)))
			break;
		fputs(buf, stdout);

		fprintf(fnetf(in), "hello %s!\n", fnetremaddr(in));
	}
	fnetclose(in);
	return 0;
}
