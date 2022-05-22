
#include "proxy.h"

//void check_job(YAAMP_JOB *job)
//{
//	if(job->coind && job->remote)
//	{
//		debuglog("error memory\n");
//	}
//}

static YAAMP_WORKER *share_find_worker(YAAMP_CLIENT *client, YAAMP_JOB *job, bool valid)
{
	for(CLI li = g_list_worker.first; li; li = li->next)
	{
		YAAMP_WORKER *worker = (YAAMP_WORKER *)li->data;
		if(worker->deleted) continue;

		if(	worker->userid == client->userid &&
			worker->workerid == client->workerid &&
			worker->valid == valid)
		{
			if(!job && !worker->coinid && !worker->remoteid)
				return worker;

			else if(!job)
				continue;

			return worker;
		}
	}

	return NULL;
}

static void share_add_worker(YAAMP_CLIENT *client, YAAMP_JOB *job, bool valid, char *ntime, double share_diff, int error_number)
{
//	check_job(job);
	g_list_worker.Enter();

	YAAMP_WORKER *worker = share_find_worker(client, job, valid);
	if(!worker)
	{
		worker = new YAAMP_WORKER;
		memset(worker, 0, sizeof(YAAMP_WORKER));

		worker->userid = client->userid;
		worker->workerid = client->workerid;
		worker->coinid = 0;
		worker->remoteid =0;
		worker->valid = valid;
		worker->error_number = error_number;
		sscanf(ntime, "%x", &worker->ntime);
		worker->share_diff = share_diff;

		worker->extranonce1 = client->extranonce1;

		g_list_worker.AddTail(worker);
	}

	if(valid)
	{
	//	worker->difficulty += (client->sharediff) / g_current_algo->diff_multiplier;
	//	client->speed += (client->sharediff) / g_current_algo->diff_multiplier * 42;
	//	client->source->speed += client->difficulty_actual / g_current_algo->diff_multiplier * 42;
	}

	g_list_worker.Leave();
}

/////////////////////////////////////////////////////////////////////////

void share_add(YAAMP_CLIENT *client, YAAMP_JOB *job, bool valid, char *extranonce2, char *ntime, char *nonce, double share_diff, int error_number)
{
//	check_job(job);
	g_shares_counter++;
	share_add_worker(client, job, valid, ntime, share_diff, error_number);

	YAAMP_SHARE *share = new YAAMP_SHARE;
	memset(share, 0, sizeof(YAAMP_SHARE));

	share->jobid = job? job->id: 0;
	strcpy(share->extranonce2, extranonce2);
	strcpy(share->ntime, ntime);
	strcpy(share->nonce, nonce);
	strcpy(share->nonce1, client->extranonce1);

	g_list_share.AddTail(share);
}

YAAMP_SHARE *share_find(int jobid, char *extranonce2, char *ntime, char *nonce, char *nonce1)
{
	g_list_share.Enter();
	for(CLI li = g_list_share.first; li; li = li->next)
	{
		YAAMP_SHARE *share = (YAAMP_SHARE *)li->data;
		if(share->deleted) continue;

		if(	share->jobid == jobid &&
			!strcmp(share->extranonce2, extranonce2) && !strcmp(share->ntime, ntime) &&
			!strcmp(share->nonce, nonce) && !strcmp(share->nonce1, nonce1))
		{
			g_list_share.Leave();
			return share;
		}
	}

	g_list_share.Leave();
	return NULL;
}
