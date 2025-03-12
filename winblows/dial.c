#include <stdio.h>
#include "fnet.h"

int ffork(void);

int
main(int argc, char** argv)
{
	NetConn* c;
	char buf[256];

	if (argc < 3) {
		printf("forgot to pass proto or address? (e.g. \"%s tcp 127.0.0.1:9999\")\n", argv[0]);
		return -1;
	}

	if(fnetinitlib() < 0){
		fprintf(stderr, "fnetinitlib: %s\n", fneterr());
		return -1;
	}

	c = fnetdial(argv[1], argv[2]);
	if (!c) {
		fprintf(stderr, "fnetdial: %s\n", fneterr());
		return -1;
	}

	printf("Connected (%s -> %s)\n", fnetlocaddr(c), fnetremaddr(c));

	if (ffork()) {
		while (fgets(buf, sizeof(buf), stdin) > 0) {
			fputs(buf, fnetf(c));
			fflush(fnetf(c));
		}
	}
	else {
		while (fgets(buf, sizeof(buf), fnetf(c)) > 0) {
			fputs(buf, stdout);
		}
	}

	fnetclose(c);
	fnetcleanlib();
	return 0;
}



#ifdef _WIN32
#include <windows.h>
#include <process.h>

static
int
ffork(void)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    char cmd[MAX_PATH];
    GetModuleFileName(NULL, cmd, MAX_PATH);

    if (!CreateProcess(cmd, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return -1; // Failure
    }

    CloseHandle(pi.hThread);
    return (int)pi.dwProcessId;
}
#else
int fork(void);
static int ffork(void) {return fork();}
#endif
