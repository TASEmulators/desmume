typedef struct _arm7comm_t
{
	int code;
	u32 offender;
	char message[1024];
} arm7comm_t;

//#define arm7comm ( (arm7comm_t*)0x02200000 )