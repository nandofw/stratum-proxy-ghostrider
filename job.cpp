
#include "proxy.h"

//client->difficulty_remote = 0;
//debuglog(" returning %x, %s, %s\n", job->id, client->sock->ip, #condition); \


#define RETURN_ON_CONDITION(condition, ret) \
	if(condition) \
	{ \
		return ret; \
	}
static int nexjob =-1;

void set_nextjob(int id)
{
	//CommonLock(&g_set_job);
	nexjob = id;
	//CommonUnlock(&g_set_job);
}
int  get_nextjob(){
	//CommonLock(&g_set_job);
	return nexjob;
	//CommonUnlock(&g_set_job);
}

static bool job_assign_client(YAAMP_JOB *job, YAAMP_CLIENT *client, double maxhash)
{
	RETURN_ON_CONDITION(client->deleted, true);
	RETURN_ON_CONDITION(client->jobid_next, true);
	RETURN_ON_CONDITION(client->jobid_locked && client->jobid_locked != job->id, true);
	RETURN_ON_CONDITION(client_find_job_history(client, job->id), true);
	client->jobid_next = job->id;
	job->count++;
	return true;
}

YAAMP_JOB *job_get_last(int coinid)
{
	//int id = get_nextjob();
	g_list_job.Enter();
	for(CLI li = g_list_job.last; li; li = li->prev)
	{
		YAAMP_JOB *job = (YAAMP_JOB *)li->data;
		if(job){
		g_list_job.Leave();
		return job;
		}
	}
	g_list_job.Leave();
	return NULL;
}
static void job_mining_notify_buffer(YAAMP_JOB *job, char *buffer)
{
YAAMP_JOB_TEMPLATE *templ = job->templ;
sprintf(buffer, "{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"%x\",\"%s\",\"%s\",\"%s\",[%s],\"%s\",\"%s\",\"%s\",true]}\n",
		job->id, templ->prevhash_be, templ->coinb1, templ->coinb2, templ->txmerkles, templ->version, templ->nbits, templ->ntime);
}

void job_send_jobid(YAAMP_CLIENT *client, int jobid){}
void job_send_last(YAAMP_CLIENT *client){
    YAAMP_JOB *job = job_get_last(0);
	if(!job) return;
	YAAMP_JOB_TEMPLATE *templ = job->templ;
	client->jobid_sent = job->id;
	char buffer[YAAMP_SMALLBUFSIZE];
	job_mining_notify_buffer(job, buffer);
	socket_send_raw(client->sock, buffer, strlen(buffer));
}
void job_broadcast(YAAMP_JOB *job)
{
	int s1 = current_timestamp_dms();
	int count = 0;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000; // max time to push to a socket (very fast)

	char buffer[YAAMP_SMALLBUFSIZE];
	job_mining_notify_buffer(job, buffer);
    
	g_list_client.Enter();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->deleted) continue;
		if(!client->sock) continue;
	//	if(client->reconnecting && client->locked) continue;
		if(client->jobid_sent == job->id) continue;

		client->jobid_sent = job->id;
		client_add_job_history(client, job->id);

		//client_adjust_difficulty(client, mf);
		client_send_difficulty(client,job->diff,1);
		//clientlog(client, "%s\r", buffer);
				
		

		setsockopt(client->sock->sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

		if (socket_send_raw(client->sock, buffer, strlen(buffer)) == -1) {
			int err = errno;
			client->broadcast_timeouts++;
			// too much timeouts, disconnect him
			if (client->broadcast_timeouts >= 3) {
				shutdown(client->sock->sock, SHUT_RDWR);
				clientlog(client, "unable to send job, sock err %d (%d times)", err, client->broadcast_timeouts);
				object_delete(client);
			}
		}
		count++;
	}

	g_list_client.Leave();
	g_last_broadcasted = time(NULL);

	int s2 = current_timestamp_dms();
	if(!count) return;

	debuglog("%s- diff %.9f job %x to %d/%d/%d clients,\n", job->name,
		job->diff, job->id, count, job->count, g_list_client.count);
}

void job_reset_clients(YAAMP_JOB *job)
{
	g_list_client.Enter();
	int id = get_nextjob();
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->deleted) continue;

		if(!job || job->id == client->jobid_next)
			client->jobid_next = id;
	}

	g_list_client.Leave();
}

void job_assign_clients(YAAMP_JOB *job, double maxhash)
{
	if (!job) return;

	job->speed = 0;
	job->count = 0;

	g_list_client.Enter();

	// pass0 locked
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->jobid_locked && client->jobid_locked != job->id) continue;

		bool b = job_assign_client(job, client, maxhash);
		if(!b) break;
	}

	// pass1 sent
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;
		if(client->jobid_sent != job->id) continue;

		bool b = job_assign_client(job, client, maxhash);
		if(!b) break;
	}
	// pass3 the rest
	for(CLI li = g_list_client.first; li; li = li->next)
	{
		YAAMP_CLIENT *client = (YAAMP_CLIENT *)li->data;

		bool b = job_assign_client(job, client, maxhash);
		if(!b) break;
	}

	g_list_client.Leave();
}

void job_assign_clients_left(double factor)
{
}

////////////////////////////////////////////////////////////////////////

pthread_mutex_t g_job_mutex;
pthread_cond_t g_job_cond;

void *job_thread(void *p)
{
	CommonLock(&g_job_mutex);
	while(!g_exiting)
	{
		job_update();
		pthread_cond_wait(&g_job_cond, &g_job_mutex);
	}
}

void job_init()
{
	pthread_mutex_init(&g_job_mutex, 0);
	pthread_cond_init(&g_job_cond, 0);

	pthread_t thread3;
	pthread_create(&thread3, NULL, job_thread, NULL);
}

void job_signal()
{
	CommonLock(&g_job_mutex);
	pthread_cond_signal(&g_job_cond);
	CommonUnlock(&g_job_mutex);
}

void job_update()
{

	g_list_job.Enter();
	YAAMP_JOB *job = job_get_last(0);
	if(job){
		job_broadcast(job);
	}
	g_list_job.Leave();

}








