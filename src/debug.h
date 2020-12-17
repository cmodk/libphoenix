#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define debug_printf(...) do { if(debug>0){printf("DEBUG from %s: Line: %d: ",__FILE__,__LINE__); printf(__VA_ARGS__);} } while(0)

#define print_fatal(...) { fprintf(stderr, "FATAL from %s: Line: %d: ",__FILE__,__LINE__); fprintf(stderr,__VA_ARGS__); fflush(stderr); exit(EXIT_FAILURE);}
#define print_error(...) { fprintf(stderr, "ERROR from %s: Line: %d: ",__FILE__,__LINE__); fprintf(stderr,__VA_ARGS__); fflush(stderr); }
#define print_warning(...) { fprintf(stderr, "WARNING from %s: Line: %d: ",__FILE__,__LINE__); fprintf(stderr,__VA_ARGS__); fflush(stderr); }
#define print_info(...) { fprintf(stderr,  "INFO  from %s: Line: %d: ",__FILE__,__LINE__); fprintf(stderr,__VA_ARGS__); }


extern int debug;
