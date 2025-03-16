/*
	Run on multiple devices in the same local network to chat with each other.
	Messages get broadcast across the entire local network.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fnet.h"

int fork(void);

int
main(int argc, char **argv)
{
	NetConn *send, *recv;
	char nick[32], buf[256], msg[256];
	int n, nick_len;

	send = fnetdial("udp", "255.255.255.255:54321");
	if(!send){
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}
	recv = fnetlisten("udp", "0.0.0.0:54321");
	if(!recv){
		fprintf(stderr, "fnetlisten: %s\n", fneterr());
		fnetclose(send);
		return -1;
	}

	do {
		printf("Enter nickname: ");
		if(fgets(nick, sizeof(nick), stdin)){
			nick[strcspn(nick, "\n")] = '\0';
		}
	} while(nick[0] == '\0' || strspn(nick, " \t") == strlen(nick));

	nick_len = strlen(nick);

	if(fork()){
		while(1){
			printf("> %s: ", nick);

			if(fgets(msg, sizeof(msg), stdin)){
				msg[strcspn(msg, "\n")] = '\0';
				if(msg[0] == '\0' || strspn(msg, " \t") == strlen(msg))
					continue;

				n = snprintf(buf, sizeof(buf), "%s: %s\n", nick, msg);
				if(n < 0 || n >= sizeof(buf))
					continue;

				fputs(buf, fnetf(send));
			}
		}
	}else{
		while(fgets(buf, sizeof(buf), fnetf(recv)) > 0){
			buf[strcspn(buf, "\n")] = '\0';
			if(buf[0] == '\0')
				continue;

			if(strncmp(buf, nick, nick_len) == 0 && buf[nick_len] == ':')
				continue;

			printf("\n< %s\n> %s: ", buf, nick);
		}
	}

	fnetclose(send);
	fnetclose(recv);
	return 0;
}
