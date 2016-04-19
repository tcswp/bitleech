extern FILE *logfp;
#define logerr(msg, ...)	fprintf(logfp, "%s:%d: " msg ": %s\n", __FILE__, __LINE__,\
							##__VA_ARGS__, strerror(errno))
#define errexit(msg, ...)	do {printf(msg, ##__VA_ARGS__); logerr(msg, ##__VA_ARGS__); exit(0);} while(0)

#ifdef DBG
#define debug_print(msg, ...)	printf(msg "\n", ##__VA_ARGS__)
#else
#define debug_print(...) (void)0
#endif
