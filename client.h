

#define YAAMP_JOB_MAXHISTORY	16

#define MIN_ADDRESS_LEN 3 /* BTC len can be as few as 26 chars, but gen. 33 or 34 */
#define MAX_ADDRESS_LEN 98 /* BITC */

class YAAMP_CLIENT: public YAAMP_OBJECT
{
public:
	YAAMP_SOCKET *sock;

	char notify_id[1024];
	int64_t reqid; // ask request id

	int created;
	int last_best;

	bool reconnectable;
	bool reconnecting;

	int userid;
	int workerid;

	int id_int;
	const char *id_str;

	char version[1024];
	char username[1024];
	char password[1024];
	char worker[1024];

	char extranonce1[32];
	int extranonce2size;

	int jobid_next;
	int jobid_sent;
	int jobid_locked;
	double difficulty;
	int job_history[YAAMP_JOB_MAXHISTORY];

	int64_t shares;
	int stats;

	int broadcast_timeouts;
};

inline void client_delete(YAAMP_OBJECT *object)
{
	YAAMP_CLIENT *client = (YAAMP_CLIENT *)object;
	if (object == NULL) return;

	socket_close(client->sock);
	delete client;

	object = NULL;
}

//////////////////////////////////////////////////////////////////////////

YAAMP_CLIENT *client_find_notify_id(const char *notify_id, bool reconnecting);

void get_next_extraonce1(char *extraonce1);
void get_next_extraonce2(char *extraonce2);
void set_extraonce1(int nonce);
void get_random_key(char *key);
void id_share(char *nextid);

void client_sort();
void client_block_ip(YAAMP_CLIENT *client, const char *reason);
void client_block_ipset(YAAMP_CLIENT *client, const char *ipset_name);

bool client_reset_multialgo(YAAMP_CLIENT *client, bool first);
bool client_initialize_multialgo(YAAMP_CLIENT *client);

void client_add_job_history(YAAMP_CLIENT *client, int jobid);
bool client_find_job_history(YAAMP_CLIENT *client, int jobid, int startat=1);

bool client_find_my_ip(const char *ip);

//////////////////////////////////////////////////////////////////////////

int client_send_difficulty(YAAMP_CLIENT *client, double difficulty, double factor);
//double client_normalize_difficulty(double difficulty);

//void client_change_difficulty(YAAMP_CLIENT *client, double difficulty, double factor);
//void client_record_difficulty(YAAMP_CLIENT *client);
//void client_adjust_difficulty(YAAMP_CLIENT *client,double factor);

//void client_initialize_difficulty(YAAMP_CLIENT *client);

//////////////////////////////////////////////////////////////////////////

int client_call(YAAMP_CLIENT *client, const char *method, const char *format, ...);
int client_ask(YAAMP_CLIENT *client, const char *method, const char *format, ...);

void client_dump_all();

int client_send_result(YAAMP_CLIENT *client, const char *format, ...);
int client_send_error(YAAMP_CLIENT *client, int error, const char *string);

bool client_ask_stats(YAAMP_CLIENT *client);

bool client_submit(YAAMP_CLIENT *client, json_value *json_params);

int client_workers_count(YAAMP_CLIENT *client);
int client_workers_byaddress(const char *username);
bool client_auth_by_workers(YAAMP_CLIENT *client);

void *client_thread(void *p);

//void db_check_user_input(char* input);

//void source_prune();

