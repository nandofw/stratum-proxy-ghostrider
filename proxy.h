
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <dirent.h>
#include <sstream>

#include <iostream>
#include <vector>

using namespace std;

#include "iniparser/src/iniparser.h"

#include "json.h"
#include "util.h"

#define YAAMP_RESTARTDELAY		(24*60*60)
#define YAAMP_MAXJOBDELAY		(2*60)
#define CURL_RPC_TIMEOUT		(30)

#define YAAMP_MS				1000
#define YAAMP_SEC				1000000

#define YAAMP_SMALLBUFSIZE		(32*1024)

#define YAAMP_NONCE_SIZE		4
#define YAAMP_RES_NONCE_SIZE	(32 - YAAMP_NONCE_SIZE)
#define YAAMP_EXTRANONCE2_SIZE	4

#define YAAMP_HASHLEN_STR		65
#define YAAMP_HASHLEN_BIN		32

extern CommonList g_list_coind;
extern CommonList g_list_client;
extern CommonList g_list_job;
extern CommonList g_list_remote;
extern CommonList g_list_renter;
extern CommonList g_list_share;
extern CommonList g_list_worker;
extern CommonList g_list_block;
extern CommonList g_list_submit;
extern CommonList g_list_source;

extern int g_tcp_port;

extern char g_tcp_server[1024];
extern int g_tcp_pool_port;
extern char g_tcp_pool_server[1024];

extern char g_user[1024];
extern char g_pass[1024];
extern bool g_handle_haproxy_ips;
extern int g_socket_recv_timeout;

extern bool g_debuglog_client;
extern bool g_debuglog_hash;
extern bool g_debuglog_socket;
extern bool g_debuglog_list;

extern bool g_connected;

extern uint64_t g_max_shares;
extern uint64_t g_shares_counter;

extern time_t g_last_broadcasted;
extern void pool_submit_share(const char *format, ...);

extern struct ifaddrs *g_ifaddr;

extern pthread_mutex_t g_db_mutex;
extern pthread_mutex_t g_nonce1_mutex;
extern pthread_mutex_t g_job_create_mutex;
extern pthread_mutex_t g_share_mutex;
extern pthread_mutex_t g_share_id_mutex;

extern volatile bool g_exiting;

#include "object.h"
#include "socket.h"
#include "client.h"
#include "job.h"
#include "share.h"
