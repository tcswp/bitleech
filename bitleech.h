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
#include <fcntl.h>			// O_NONBLOCK
#include <poll.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>

#include "log.h"
#include "metainfo.h"
#include "file.h"
#include "bdecode.h"
#include "peer.h"
#include "tracker.h"
#include "queue.h"
#include "base64.h"
