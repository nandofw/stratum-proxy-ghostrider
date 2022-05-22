
#include "proxy.h"

static int g_extraonce1_counter = 0;
static int g_extraonce2_counter = 0;
static int g_share_id=2;

void get_next_extraonce1(char *extraonce1)
{
	CommonLock(&g_nonce1_mutex);

	sprintf(extraonce1, "%08x", g_extraonce1_counter|0x81000000);

	CommonUnlock(&g_nonce1_mutex);
}
void get_next_extraonce2(char *extraonce2)
{
	CommonLock(&g_nonce1_mutex);

	g_extraonce2_counter++;
	sprintf(extraonce2, "%08x", g_extraonce2_counter|0x10000000);

	CommonUnlock(&g_nonce1_mutex);
}
void set_extraonce1(int nonce)
{
	CommonLock(&g_nonce1_mutex);

	g_extraonce1_counter = nonce;
	
	CommonUnlock(&g_nonce1_mutex);
}

void id_share(char *nextid)
{
	CommonLock(&g_share_id_mutex);
	g_share_id++;
	sprintf(nextid, "%i", g_share_id);
	CommonUnlock(&g_share_id_mutex);
}

void get_random_key(char *key)
{
	int i1 = rand();
	int i2 = rand();
	int i3 = rand();
	int i4 = rand();
	sprintf(key, "%08x%08x%08x%08x", i1, i2, i3, i4);
}

YAAMP_CLIENT *client_find_notify_id(const char *notify_id, bool reconnecting)
{
	if (!notify_id || !strlen(notify_id))
		return NULL;

	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->reconnecting == reconnecting && !strcmp(client->notify_id, notify_id))
		{
			g_list_client.Leave();
			return client;
		}
	}

	g_list_client.Leave();
	return NULL;
}

void client_sort()
{
	
}

int client_send_error(YAAMP_CLIENT *client, int error, const char *string)
{
	char buffer3[1024];

	if(client->id_str)
		sprintf(buffer3, "\"%s\"", client->id_str);
	else
		sprintf(buffer3, "%d", client->id_int);

	return socket_send(client->sock, "{\"id\":%s,\"result\":false,\"error\":[%d,\"%s\",null]}\n", buffer3, error, string);
}

int client_send_result(YAAMP_CLIENT *client, const char *format, ...)
{
	char buffer[YAAMP_SMALLBUFSIZE];
	va_list args;

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	char buffer3[1024];

	if(client->id_str)
		sprintf(buffer3, "\"%s\"", client->id_str);
	else
		sprintf(buffer3, "%d", client->id_int);

	return socket_send(client->sock, "{\"id\":%s,\"result\":%s,\"error\":null}\n", buffer3, buffer);
}

int client_call(YAAMP_CLIENT *client, const char *method, const char *format, ...)
{
	char buffer[YAAMP_SMALLBUFSIZE];
	va_list args;

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	return socket_send(client->sock, "{\"id\":null,\"method\":\"%s\",\"params\":%s}\n", method, buffer);
}

int client_ask(YAAMP_CLIENT *client, const char *method, const char *format, ...)
{
	char buffer[YAAMP_SMALLBUFSIZE];
	va_list args;
	int64_t id = client->shares;

	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	int ret = socket_send(client->sock, "{\"id\":%d,\"method\":\"%s\",\"params\":%s}\n", id, method, buffer);
	if (ret == -1) {
		debuglog("unable to ask %s\n", method);
		return 0; // -errno
	}
	client->reqid = id;
	return id;
}

void client_block_ip(YAAMP_CLIENT *client, const char *reason)
{
	char buffer[1024];
	sprintf(buffer, "iptables -A INPUT -s %s -p tcp --dport %d -j REJECT", client->sock->ip, g_tcp_port);
	if(strcmp("0.0.0.0", client->sock->ip) == 0) return;
	if(strstr(client->sock->ip, "192.168.")) return;
	if(strstr(client->sock->ip, "127.0.0.")) return;

	int s = system(buffer);
	stratumlog("%s blocked (%s)\n", client->sock->ip, reason);
}

void client_block_ipset(YAAMP_CLIENT *client, const char *ipset_name)
{
	char buffer[1024];
	sprintf(buffer, "ipset -q -A %s %s", ipset_name, client->sock->ip);
	if(strcmp("0.0.0.0", client->sock->ip) == 0) return;
	if(strstr(client->sock->ip, "192.168.")) return;
	if(strstr(client->sock->ip, "127.0.0.")) return;

	int s = system(buffer);
	stratumlog("%s blocked via ipset %s %s\n", client->sock->ip, ipset_name, client->username);
}


void client_add_job_history(YAAMP_CLIENT *client, int jobid)
{
	if(!jobid)
	{
		debuglog("trying to add jobid 0\n");
		return;
	}

	bool b = client_find_job_history(client, jobid, 0);
	if(b)
	{
//		debuglog("ERROR history already added job %x\n", jobid);
		return;
	}

	for(int i=YAAMP_JOB_MAXHISTORY-1; i>0; i--)
		client->job_history[i] = client->job_history[i-1];

	client->job_history[0] = jobid;
}

bool client_find_job_history(YAAMP_CLIENT *client, int jobid, int startat)
{
	for(int i=startat; i<YAAMP_JOB_MAXHISTORY; i++)
	{
		if(client->job_history[i] == jobid)
		{
//			if(!startat)
//				debuglog("job %x already sent, index %d\n", jobid, i);

			return true;
		}
	}

	return false;
}

int hostname_to_ip(const char *hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;

    if(hostname[0]>='0' && hostname[0]<='9')
    {
    	strcpy(ip, hostname);
    	return 0;
    }

    if ( (he = gethostbyname( hostname ) ) == NULL)
    {
        // get the host info
        herror("gethostbyname");
        return 1;
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    for(i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strcpy(ip, inet_ntoa(*addr_list[i]));
        return 0;
    }

    return 1;
}

bool client_find_my_ip(const char *name)
{
//	return false;
	char ip[1024] = "";

	hostname_to_ip(name, ip);
	if(!ip[0]) return false;

	char host[NI_MAXHOST];
	for(struct ifaddrs *ifa = g_ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if(ifa->ifa_addr == NULL) continue;
		host[0] = 0;

		getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if(!host[0]) continue;

		if(!strcmp(host, ip))
		{
			debuglog("found my ip %s\n", ip);
			return true;
		}
	}

	return false;
}







