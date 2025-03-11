/* started 2025.03.08 - MIT - https://github.com/skullchap/fnet */

#define nil 		((void*)0)
#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))
#define funcname	(__func__)

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned long	ulong;

int	setfneterr(char *fmt, ...);
char*	errnostr(void);

/* clib */
typedef struct FILE FILE;

int	printf(const char *, ...);
int	fprintf(FILE *, const char *, ...);
int	snprintf(char *, ulong, const char *, ...);
int	sscanf(const char *, const char *, ...);;
FILE*	fopen(const char *, const char *);
int	setvbuf(FILE *, char *, int, ulong);
ulong	fread(void *ptr, ulong size, ulong nitems, FILE*);
int	fputs(const char *, FILE* );
int	fseek(FILE*, long off, int whence);
long	ftell(FILE*);
void	rewind(FILE*);
void	fclose(FILE*);
int	fflush(FILE *);
void*	alloca(ulong);
void*	malloc(ulong);
void*	realloc(void *, ulong);
void	free(void *);
void*	calloc(ulong n, ulong sz);
void*	memcpy(void *, const void *, ulong);
void*	memset(void *, int, ulong);
int	memcmp(const void *, const void *, ulong);
void*	memchr(const void *, int, ulong);
ulong	strlen(const char *);
char*	strncpy(char *, const char *, ulong);
int	strncmp(const char *, const char *, ulong);
char*	strchr(const char *, int);
char*	strrchr(const char *, int);
char*	strerror(int);
int	atoi(const char *str);

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