
class YAAMP_WORKER: public YAAMP_OBJECT
{
public:
	int userid;
	int workerid;
	int coinid;
	int remoteid;

	bool valid;
	bool extranonce1;
	int32_t error_number;

	uint32_t ntime;
	double difficulty;
	double share_diff; /* submitted hash diff */
};

inline void worker_delete(YAAMP_OBJECT *object)
{
	YAAMP_WORKER *worker = (YAAMP_WORKER *)object;
	delete worker;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

class YAAMP_SHARE: public YAAMP_OBJECT
{
public:
	int jobid;
	char extranonce2[64];
	char ntime[32];
	char nonce[64];
	char nonce1[64];
};

inline void share_delete(YAAMP_OBJECT *object)
{
	YAAMP_SHARE *share = (YAAMP_SHARE *)object;
	delete share;
}

//YAAMP_WORKER *share_find_worker(int userid, int workerid, int coinid, bool valid);
//void share_add_worker(int userid, int workerid, int coinid, bool valid, double difficulty);

///////////

YAAMP_SHARE *share_find(int jobid, char *extranonce2, char *ntime, char *nonce, char *nonce1);
void share_add(YAAMP_CLIENT *client, YAAMP_JOB *job, bool valid, char *extranonce2, char *ntime, char *nonce, double share_diff, int error_number);

class YAAMP_SUBMIT: public YAAMP_OBJECT
{
public:
	time_t created;
	bool valid;
	int remoteid;
	double difficulty;
};

inline void submit_delete(YAAMP_OBJECT *object)
{
	YAAMP_SUBMIT *submit = (YAAMP_SUBMIT *)object;
	delete submit;
}





