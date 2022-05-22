
#include "proxy.h"
#include <signal.h>
#include <sys/resource.h>

CommonList g_list_client;
CommonList g_list_job;
CommonList g_list_share;
CommonList g_list_worker;
CommonList g_list_submit;


int g_tcp_port;
char g_tcp_server[1024];
int g_tcp_pool_port;
char g_tcp_pool_server[1024];

char g_user[1024];
char g_pass[1024];

bool g_handle_haproxy_ips = false;
int g_socket_recv_timeout = 600;

bool g_debuglog_client;
bool g_debuglog_hash;
bool g_debuglog_socket;
bool g_debuglog_rpc;
bool g_debuglog_list;
bool g_debuglog_remote;

bool g_connected;

uint64_t g_max_shares = 0;
uint64_t g_shares_counter = 0;
uint64_t g_shares_log = 0;



bool g_allow_rolltime = true;
time_t g_last_broadcasted = 0;

pthread_mutex_t g_db_mutex;
pthread_mutex_t g_nonce1_mutex;
pthread_mutex_t g_job_create_mutex;
pthread_mutex_t g_share_mutex;
pthread_mutex_t g_share_id_mutex;

struct ifaddrs *g_ifaddr;

volatile bool g_exiting = false;

void *stratum_thread(void *p);
void *monitor_thread(void *p);
void *proxy_thread(void *p);
YAAMP_SOCKET *pool_socket;


int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("usage: %s <config>\n", argv[0]);
		return 1;
	}
	srand(time(NULL));
	getifaddrs(&g_ifaddr);

	initlog(argv[1]);

	char configfile[1024];
	sprintf(configfile, "%s.conf", argv[1]);

	dictionary *ini = iniparser_load(configfile);
	if(!ini)
	{
		debuglog("cant load config file %s\n", configfile);
		return 1;
	}

	g_tcp_port = iniparser_getint(ini, "TCP:port", 3333);
	strcpy(g_tcp_server, iniparser_getstring(ini, "TCP:server", NULL));
    g_tcp_pool_port = iniparser_getint(ini, "POOL:port", 7777);
	strcpy(g_tcp_pool_server, iniparser_getstring(ini, "POOL:server", ""));

	
	g_socket_recv_timeout = iniparser_getint(ini, "STRATUM:recv_timeout", 600);
	g_max_shares = iniparser_getint(ini, "STRATUM:max_shares", g_max_shares);

    strcpy(g_user, iniparser_getstring(ini, "CLIENT:user", NULL));
	strcpy(g_pass, iniparser_getstring(ini, "CLIENT:pass", NULL));
	
	g_debuglog_client = iniparser_getint(ini, "DEBUGLOG:client", false);
	g_debuglog_hash = iniparser_getint(ini, "DEBUGLOG:hash", false);
	g_debuglog_socket = iniparser_getint(ini, "DEBUGLOG:socket", false);

	iniparser_freedict(ini);




	struct rlimit rlim_threads = {0x8000, 0x8000};
	setrlimit(RLIMIT_NPROC, &rlim_threads);

	

	yaamp_create_mutex(&g_db_mutex);
	yaamp_create_mutex(&g_nonce1_mutex);
	yaamp_create_mutex(&g_job_create_mutex);
    yaamp_create_mutex(&g_share_mutex);
	yaamp_create_mutex(&g_share_id_mutex);

	sleep(2);
	job_init();

	//job_signal();

	////////////////////////////////////////////////

	g_connected = false;
    stratumlogdate("starting stratum-proxy for %s:%d\n",
	g_tcp_pool_server, g_tcp_pool_port);
    pthread_t thread3;
	pthread_create(&thread3, NULL, proxy_thread, NULL);
	while(!g_connected){
		sleep(1);
	}
	stratumlogdate("starting Proxy pool %s:%d\n",
	g_tcp_server, g_tcp_port);
	pthread_t thread1;
	pthread_create(&thread1, NULL, monitor_thread, NULL);
	pthread_t thread2;
	pthread_create(&thread2, NULL, stratum_thread, NULL);

	sleep(20);

	while(!g_exiting)
	{
		sleep(1);
		job_signal();

//		source_prune();

		object_prune(&g_list_job, job_delete);
		object_prune(&g_list_client, client_delete);
		object_prune(&g_list_worker, worker_delete);
		object_prune(&g_list_share, share_delete);
		object_prune(&g_list_submit, submit_delete);

		if (!g_exiting) sleep(20);
	}

	//pthread_join(thread2, NULL);

	closelogs();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void *monitor_thread(void *p)
{
	while(!g_exiting)
	{
		sleep(120);

		if(g_last_broadcasted + YAAMP_MAXJOBDELAY < time(NULL))
		{
			g_exiting = true;
			stratumlogdate("dead lock, exiting...\n");
			exit(1);
		}

		if(g_max_shares && g_shares_counter) 
		{

			if((g_shares_counter - g_shares_log) > 10000) 
			{
				stratumlogdate("%luK shares...\n", (g_shares_counter/1000u));
				g_shares_log = g_shares_counter;
			}

			if(g_shares_counter > g_max_shares) 
			{
				g_exiting = true;
				stratumlogdate("need a restart (%lu shares), exiting...\n", (unsigned long) g_max_shares);
				exit(1);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void *stratum_thread(void *p)
{
	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock <= 0) yaamp_error("socket");

	int optval = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	struct sockaddr_in serv;

	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(g_tcp_port);

	int res = bind(listen_sock, (struct sockaddr*)&serv, sizeof(serv));
	if(res < 0) yaamp_error("bind");

	res = listen(listen_sock, 4096);
	if(res < 0) yaamp_error("listen");

	/////////////////////////////////////////////////////////////////////////

	int failcount = 0;
	while(!g_exiting)
	{
		int sock = accept(listen_sock, NULL, NULL);
		if(sock <= 0)
		{
			int error = errno;
			stratumlog("socket accept() error %d\n", error);
			failcount++;
			usleep(50000);
			if (error == 24 && failcount > 5) {
				g_exiting = true; // happen when max open files is reached (see ulimit)
				stratumlogdate("too much socket failure, exiting...\n");
				exit(error);
			}
			continue;
		}

		failcount = 0;
		pthread_t thread;
		int res = pthread_create(&thread, NULL, client_thread, (void *)(long)sock);
		if(res != 0)
		{
			int error = errno;
			close(sock);
			g_exiting = true;
			stratumlog("pthread_create error %d %d\n", res, error);
		}

		pthread_detach(thread);
	}
}


void *proxy_thread(void *p)
{
	debuglog("stratum proxy trhead\n");
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		debuglog("failed to create socket\n");
		pthread_exit(NULL);
	}
	
    struct hostent *ent = gethostbyname(g_tcp_pool_server);
	if(ent){
	struct sockaddr_in serv;
	serv.sin_family = AF_INET;
	serv.sin_port = htons(g_tcp_pool_port);
    bcopy((char *)ent->h_addr, (char *)&serv.sin_addr.s_addr, ent->h_length);
	//inet_aton(ip,&serv.sin_addr);
	debuglog("connecting to %s:%i\n", g_tcp_pool_server, g_tcp_pool_port);
    int res = connect(sock, (struct sockaddr*)&serv, sizeof(serv));
	if(res >= 0){
	debuglog("connected to %s:%i\n", g_tcp_pool_server, g_tcp_pool_port);
		
	
	pool_socket = socket_initialize(sock);
  //  debuglog("connected to %s:%s\n", g_tcp_pool_server, g_tcp_pool_port);
	/////////////////////////////////////////////////////////////////////////

	int accepted = 0;
	int rejected = 0;
    const char message_subscribe[] = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"stratum-proxy/0.0.1\"]}\n";
	char message_authorize[2*1024];
	sprintf(message_authorize, "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n",
				g_user, g_pass);
	double diff = 1.0;
	while(!g_exiting)
	{
		  socket_send(pool_socket,message_subscribe);
          socket_send(pool_socket,message_authorize);
          while (!g_exiting){
                json_value *json = socket_nextjson(pool_socket);
                if(json)
		        {
                    int id = json_get_int(json,"id");
                    const char *method = json_get_string(json, "method");
					//debuglog("method %s:%i\n", method,id);
                    if(id == 1)
                    {
                        //save id and extranonce1
						try{
						json_value *json_result = json_get_array(json, "result");
						debuglog("nonce %s\n", json_result->u.array.values[1]->u.string.ptr);
                        set_extraonce1(strtol(json_result->u.array.values[1]->u.string.ptr,nullptr,16));
						}catch(...){
						debuglog("bug error\n");	
						}
                    }
                    else if(id == 2)
                    {
                        if(json_get_bool(json,"result")){
						debuglog("Miner authorized\n");	
						g_connected =true;	
						}else{
						debuglog("Miner rejected: \n");	
						debuglog("No active pool.\n");
						exit(1);
						}
							
                    }
                    else 
                    {
					if (id > 2){
						if(json_get_bool(json,"result")){
							accepted++;
							debuglog("share ACCEPTED (%i/%i)\n",accepted,rejected);
						}else{
							rejected++;
							debuglog("share REJECTED (%i/%i) reason %s\n",accepted,rejected,json_get_array(json, "error")->u.array.values[1]->u.string.ptr);
						}
					}else{
					json_value *json_params = json_get_array(json, "params");
					if(!strcmp(method, "mining.set_difficulty")){
						diff = json_params->u.array.values[0]->u.dbl;
					}
					else
					if(!strcmp(method, "mining.notify")){
						try{
					YAAMP_JOB *job = new YAAMP_JOB;
					YAAMP_JOB_TEMPLATE *templ= new YAAMP_JOB_TEMPLATE;
					const char *p0 = json_params->u.array.values[1]->u.string.ptr;
					strcpy(templ->prevhash_be, p0);
					const char *p2 = json_params->u.array.values[2]->u.string.ptr;
					strcpy(templ->coinb1, p2);
					const char *p3 = json_params->u.array.values[3]->u.string.ptr;
					strcpy(templ->coinb2, p3);
					json_value *params = json_params->u.array.values[4];
					
					templ->txmerkles[0] = '\0';
						json_value **x;
						for (x = params->u.array.begin() ; x != params->u.array.end(); ++x){
							sprintf(templ->txmerkles + strlen(templ->txmerkles), "\"%s\",", (*x)->u.string.ptr);
						}
						if(templ->txmerkles[0])
							templ->txmerkles[strlen(templ->txmerkles)-1] = 0;
					
					const char *p5 = json_params->u.array.values[5]->u.string.ptr;
					strcpy(templ->version, p5);
					const char *p6 = json_params->u.array.values[6]->u.string.ptr;
					strcpy(templ->nbits, p6);
					const char *p7 = json_params->u.array.values[7]->u.string.ptr;
					strcpy(templ->ntime, p7);
					job->templ= templ;
					job->id =strtol(json_params->u.array.values[0]->u.string.ptr,nullptr,16);
					job->diff=diff;
					debuglog("job id: %i\n", job->id);
					set_nextjob(job->id);
					g_list_job.AddTail(job);
					job_signal();}catch(...){debuglog("failed to decode job");}
					}
					}
                    }
				json_value_free(json);	
                }else{
				debuglog("bad json Restarting");
				exit(1);	
				}
          }
		}
    }
	}
	pthread_exit(NULL);
}

void pool_submit_share(const char *format, ...){
	try{
	socket_send(pool_socket,format);
	}catch(...){
	debuglog("failed to send share");	
	}
}