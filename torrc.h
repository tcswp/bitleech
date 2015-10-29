#include <stdio.h>
#include <stdlib.h> 		// exit() and stuff
#include <time.h>				// for rand() n stuff
#include <ctype.h>  		// isdigit()
#include <string.h>
#include <errno.h>
#include <unistd.h>			// filestream constants
#include <time.h>
#include <signal.h>
#include <math.h>
#include <netinet/in.h>	// socket stuff
#include <sys/socket.h> // more socket stuff
#include <sys/types.h>
#include <sys/stat.h> // check if dir exists
#include <netdb.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <fcntl.h>			// O_NONBLOCK
#include <poll.h>
#include <stdbool.h>
#include <assert.h>

#include "metainfo.h"
#include "file.h"
#include "bdecode.h"
#include "tracker.h"
#include "peer.h"
#include "queue.h"

#ifdef DBG
#define DEBUG(msg, ...)	fprintf(stderr, "%s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#else
#define DEBUG(msg, ...)
#endif
