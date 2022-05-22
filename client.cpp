
#include "proxy.h"

bool client_suggest_difficulty(YAAMP_CLIENT *client, json_value *json_params)
{
	client_send_result(client, "true");
	return true;
}

bool client_suggest_target(YAAMP_CLIENT *client, json_value *json_params)
{
	client_send_result(client, "true");
	return true;
}

bool client_subscribe(YAAMP_CLIENT *client, json_value *json_params)
{
	//if(client_find_my_ip(client->sock->ip)) return false;
	get_next_extraonce1(client->extranonce1);

	client->extranonce2size = YAAMP_EXTRANONCE2_SIZE;

	get_random_key(client->notify_id);

	if(json_params->u.array.length>0)
	{
		if (json_params->u.array.values[0]->u.string.ptr)
			strncpy(client->version, json_params->u.array.values[0]->u.string.ptr, 1023);

		
		if(strstr(client->version, "proxy") || strstr(client->version, "/3."))
        client->reconnectable = false;

		if(strstr(client->version, "ccminer")) client->stats = true;
		if(strstr(client->version, "cpuminer-multi")) client->stats = true;
		if(strstr(client->version, "cpuminer-opt")) client->stats = true;
	}

	if(json_params->u.array.length>1)
	{
		char notify_id[1024] = { 0 };
		if (json_params->u.array.values[1]->u.string.ptr)
			strncpy(notify_id, json_params->u.array.values[1]->u.string.ptr, 1023);

		YAAMP_CLIENT *client1 = client_find_notify_id(notify_id, true);
		if(client1)
		{
			strncpy(client->notify_id, notify_id, 1023);

			client->jobid_locked = client1->jobid_locked;
	
			client->extranonce2size = client1->extranonce2size;
			strcpy(client->extranonce1, client1->extranonce1);

			memcpy(client->job_history, client1->job_history, sizeof(client->job_history));
			client1->lock_count = 0;

			if (g_debuglog_client) {
				debuglog("reconnecting client locked to %x\n", client->jobid_next);
			}
		}

		else
		{
			YAAMP_CLIENT *client1 = client_find_notify_id(notify_id, false);
			if(client1)
			{
				strncpy(client->notify_id, notify_id, 1023);
				
				memcpy(client->job_history, client1->job_history, sizeof(client->job_history));
				client1->lock_count = 0;

				if (g_debuglog_client) {
					debuglog("reconnecting2 client\n");
				}
			}
		}
	}
	char extranonce2[64];
	get_next_extraonce2(extranonce2);
	//if (g_debuglog_client) {
		debuglog("new client with extranonce %s\n", extranonce2);
	//}
	
	client_send_result(client, "[[[\"mining.set_difficulty\",\"%.3g\"],[\"mining.notify\",\"%s\"]],\"%s\",%d,\"%s\"]",
		0.4, client->notify_id, client->extranonce1, client->extranonce2size,extranonce2);

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////

bool client_authorize(YAAMP_CLIENT *client, json_value *json_params)
{
	if(g_list_client.Find(client)) {
		clientlog(client, "Already logged");
		client_send_error(client, 21, "Already logged");
		return false;
	}

	if(json_params->u.array.length>1 && json_params->u.array.values[1]->u.string.ptr)
		strncpy(client->password, json_params->u.array.values[1]->u.string.ptr, 1023);

	if (g_list_client.count >= 5000) {
		client_send_error(client, 21, "Server full");
		return false;
	}

	if(json_params->u.array.length>0 && json_params->u.array.values[0]->u.string.ptr)
	{
		strncpy(client->username, json_params->u.array.values[0]->u.string.ptr, 1023);

		//db_check_user_input(client->username);
		int len = strlen(client->username);
		if (!len)
			return false;

		char *sep = strpbrk(client->username, ".,;:");
		if (sep) {
			*sep = '\0';
			strncpy(client->worker, sep+1, 1023-len);
			if (strlen(client->username) > MAX_ADDRESS_LEN) return false;
		} else if (len > MAX_ADDRESS_LEN) {
			return false;
		}
	}

	//client_initialize_difficulty(client);

	if (g_debuglog_client) {
		debuglog("new client %s, %s, %s\n", client->username, client->password, client->version);
	}
	
	client_send_result(client, "true");
	YAAMP_JOB *job = job_get_last(0);
	if(job){
	client->jobid_sent=job->id;
	client->difficulty=job->diff;	
	client_send_difficulty(client, job->diff,1);
	job_send_last(client);
	}
	g_list_client.AddTail(client);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

int client_workers_count(YAAMP_CLIENT *client)
{
	int count = 0;
	if (!client || client->userid <= 0)
		return count;

	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *cli = (YAAMP_CLIENT *)li->data;
		if (cli->deleted) continue;
		if (cli->userid == client->userid) count++;
	}
	g_list_client.Leave();

	return count;
}

int client_workers_byaddress(const char *username)
{
	int count = 0;
	if (!username || !strlen(username))
		return count;

	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *cli = (YAAMP_CLIENT *)li->data;
		if (cli->deleted) continue;
		if (strcmp(cli->username, username) == 0) count++;
	}
	g_list_client.Leave();

	return count;
}

bool client_auth_by_workers(YAAMP_CLIENT *client)
{
	if (!client || client->userid < 0)
		return false;

	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *cli = (YAAMP_CLIENT *)li->data;
		if (cli->deleted) continue;
		if (client->userid) {
			if(cli->userid == client->userid) {
				break;
			}
		} else if (strcmp(cli->username, client->username) == 0) {
			client->userid = cli->userid;
			break;
		}
	}
	g_list_client.Leave();

	return (client->userid > 0);
}
static bool valid_string_params(json_value *json_params)
{
	for(int p=0; p < json_params->u.array.length; p++) {
		if (!json_is_string(json_params->u.array.values[p]))
			return false;
	}
	return true;
}

int client_send_difficulty(YAAMP_CLIENT *client, double difficulty, double factor){
	if(client->difficulty != difficulty){
		client->difficulty = difficulty;
	if(difficulty >= 1)
		client_call(client, "mining.set_difficulty", "[%.3f]", (difficulty));
	else
		client_call(client, "mining.set_difficulty", "[%0.8f]", (difficulty));
	return 0;
	}
}


bool client_submit(YAAMP_CLIENT *client, json_value *json_params)
{
	if(json_params->u.array.length<5 || !valid_string_params(json_params)) {
		debuglog("%s - %s bad message\n", client->username, client->sock->ip);
		//client->submit_bad++;
		return false;
	}

	char extranonce1[32] = { 0 };
	char extranonce2[32] = { 0 };
	char extra[160] = { 0 };
	char nonce[80] = { 0 };
	char ntime[32] = { 0 };
	char id[8] = { 0 };

	if (strlen(json_params->u.array.values[1]->u.string.ptr) > 32) {
	clientlog(client, "bad json, wrong jobid len");
	//client->submit_bad++;
	return false;
	}
	strncpy(id, json_params->u.array.values[1]->u.string.ptr, 31);
	//int jobid = htoi(json_params->u.array.values[1]->u.string.ptr);
	strncpy(extranonce2, json_params->u.array.values[2]->u.string.ptr, 31);
	strncpy(ntime, json_params->u.array.values[3]->u.string.ptr, 31);
	strncpy(nonce, json_params->u.array.values[4]->u.string.ptr, 31);
	//strncpy(extranonce1, json_params->u.array.values[5]->u.string.ptr, 31);
	
	//string_lower(extranonce1);
	string_lower(extranonce2);
	string_lower(ntime);
	string_lower(nonce);
	//proxy_submit(extranonce1,extranonce2,ntime,nonce);
	char buffer[YAAMP_SMALLBUFSIZE];
	char nextid[8]={0};
	id_share(nextid);
	sprintf(buffer,"{\"id\":%s,\"jsonrpc\":\"2.0\",\"method\":\"mining.submit\",\"params\":\
	[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",nextid,g_user,id,extranonce2,ntime,nonce);
	pool_submit_share(buffer);
	client_send_result(client, "true");
	//share_add(client, nullptr , true, extranonce2, ntime, nonce, 0, 0);
    return true;
}


void *client_thread(void *p)
{
	YAAMP_CLIENT *client = new YAAMP_CLIENT;
	if(!client) {
		stratumlog("client_thread OOM");
		pthread_exit(NULL);
		return NULL;
	}
	memset(client, 0, sizeof(YAAMP_CLIENT));

	client->reconnectable = true;;
	client->created = time(NULL);
	client->last_best = time(NULL);

	client->sock = socket_initialize((int)(long)p);

//	usleep(g_list_client.count * 5000);
	try{
	while(!g_exiting)
	{
		
		json_value *json = socket_nextjson(client->sock, client);
		if(!json)
		{
			clientlog(client, "bad json");
			break;
		}

		client->id_int = json_get_int(json, "id");
		client->id_str = json_get_string(json, "id");
		if (client->id_str && strlen(client->id_str) > 32) {
			clientlog(client, "bad id");
			break;
		}

		const char *method = json_get_string(json, "method");

		if(!method)
		{
			json_value_free(json);
			clientlog(client, "bad json, no method");
			break;
		}

		json_value *json_params = json_get_array(json, "params");
		if(!json_params)
		{
			json_value_free(json);
			clientlog(client, "bad json, no params");
			break;
		}

		if (g_debuglog_client) {
			debuglog("client %s %d %s\n", method, client->id_int, client->id_str? client->id_str: "null");
		}

		bool b = false;
		if(!strcmp(method, "mining.subscribe"))
			b = client_subscribe(client, json_params);

		else if(!strcmp(method, "mining.authorize"))
			b = client_authorize(client, json_params);

		else if(!strcmp(method, "mining.ping"))
			b = client_send_result(client, "\"pong\"");

		else if(!strcmp(method, "mining.submit"))
			b = client_submit(client, json_params);
			
		else if(!strcmp(method, "mining.suggest_difficulty"))
			b = client_suggest_difficulty(client, json_params);

		else if(!strcmp(method, "mining.suggest_target"))
			b = client_suggest_target(client, json_params);

		else if(!strcmp(method, "mining.get_transactions"))
			b = client_send_result(client, "[]");

		else if(!strcmp(method, "mining.multi_version"))
			b = client_send_result(client, "false"); // ASICBOOST

		else if(!strcmp(method, "mining.extranonce.subscribe"))
		{
			b = client_send_error(client, 20, "Not supported");
			//b = client_send_result(client, "true");
		}

		else if(!strcmp(method, "getwork"))
		{
			clientlog(client, "using getwork"); // client using http:// url
		}
		else
		{
			b = client_send_error(client, 20, "Not supported");
			//client->submit_bad++;

			stratumlog("unknown method %s %s\n", method, client->sock->ip);
		}

		json_value_free(json);
		if(!b) break;
	}
	}catch (...){
		debuglog("excception occured terminating\n");
	}
//	source_close(client->source);

	if (g_debuglog_client) {
		debuglog("client terminate\n");
	}
	if(!client) {
		pthread_exit(NULL);
	}

	else if(client->sock->total_read == 0)
		clientlog(client, "no data");

	if(client->sock->sock >= 0)
		shutdown(client->sock->sock, SHUT_RDWR);

	if(g_list_client.Find(client))
	{
		object_delete(client);
	} else {
		// only clients sockets in g_list_client are purged (if marked deleted)
		socket_close(client->sock);
		delete client;
	}

	pthread_exit(NULL);
}
