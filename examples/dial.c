#include <stdio.h>
#include "fnet.h"

int fork(void);

int
main(int argc, char **argv)
{
	NetConn *c;
	char buf[256];

	if(argc < 3){
		printf("forgot to pass proto or address? (e.g. \"%s tcp 127.0.0.1:9999\")\n", argv[0]);
		return -1;
	}

	c = fnetdial(argv[1], argv[2]);
	if(!c){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	printf("Connected (%s -> %s)\n", fnetlocaddr(c), fnetremaddr(c));

	if(fork()){
		while(fgets(buf, sizeof(buf), stdin)){
			fputs(buf, fnetf(c));
		}
	}else{
		while(fgets(buf, sizeof(buf), fnetf(c))){
			fputs(buf, stdout);
		}
	}

	fnetclose(c);
	return 0;
}
