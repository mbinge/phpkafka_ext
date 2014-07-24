#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/time.h>
#include <errno.h>
#include <syslog.h>
#include <time.h>
#include <limits.h>
#include "library.h"
#include "librdkafka/rdkafka.h"

#define MAX_INSTANCE 10
static rd_kafka_t* instances[MAX_INSTANCE]={NULL};

static long gen_handler()
{
	long i=0;
	long handler = -1;
	for(i=0; i<MAX_INSTANCE; i++)
	{
		if(instances[i]==NULL)
		{
			handler = i;
			break;
		}
	}
	return handler;
}

static long set_rd_kafka_t(long handler, rd_kafka_t *rk)
{
	if(handler<MAX_INSTANCE){
		instances[handler] = rk;
		return 1;
	} else {
		return 0;
	}
}

static rd_kafka_t* get_rd_kafka_t(long handler)
{
	if(handler<MAX_INSTANCE)return instances[handler];
	return NULL;
}

static void release_rd_kafka_t(long handler)
{
	if(handler < MAX_INSTANCE)instances[handler]=NULL;
}

static rd_kafka_topic_t* topic_instances[MAX_INSTANCE]={NULL};

static long set_rd_kafka_topic_t(long handler, rd_kafka_topic_t* topic)
{
	if(handler < MAX_INSTANCE) {
		topic_instances[handler] = topic;
		return 1;
	} else {
		return 0;
	}
}

static rd_kafka_topic_t* get_rd_kafka_topic_t(long handler)
{
	if(handler < MAX_INSTANCE)return topic_instances[handler];
	return NULL;
}

static void release_rd_kafka_topic_t(long handler)
{
	if(handler < MAX_INSTANCE)topic_instances[handler]=NULL;
}

typedef struct {
	long err;
	long handler;
	char msg[512];
} shared_msg;

static shared_msg* msg_instances[MAX_INSTANCE] = {NULL};

static long set_shared_msg(long instance, long err, const char* msg)
{
	if(instance >= MAX_INSTANCE)return 0;
	shared_msg *sh_msg;
	if(msg_instances[instance]==NULL){
		sh_msg = calloc(1, sizeof(*sh_msg));
		msg_instances[instance] = sh_msg;
	} else {
		sh_msg = msg_instances[instance];
	}
	sh_msg->handler = instance;
	sh_msg->err = err;
	strcpy(sh_msg->msg, msg);
	return 1;
}
static shared_msg* get_shared_msg(long instance)
{
	if(instance >= MAX_INSTANCE)return NULL;
	return msg_instances[instance];
}
static void release_shared_msg(long instance)
{
	if(instance<MAX_INSTANCE)free(msg_instances[instance]);
}


void error_callback(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
	if(err != RD_KAFKA_RESP_ERR_NO_ERROR)
	{
		long handler = (intptr_t)opaque;
		set_shared_msg(handler, err, reason);
	}
	printf("error_callback:%d,%s\n",err ,reason);
}

void put_callback(rd_kafka_t *rk, void *payload, size_t len, rd_kafka_resp_err_t error_code, void *opaque, void *msg_opaque)
{
	long handler = (intptr_t)opaque;
	char tmp[512] = {0};
        if (error_code){
                sprintf(tmp, "Message delivery failed: %s", rd_kafka_err2str(error_code));
        } else { 
        	sprintf(tmp, "Message delivered (%zd bytes)\n", len);
	}
	set_shared_msg(handler, error_code, tmp);
}

int stats_callback(rd_kafka_t *rk, char *json, size_t json_len, void *opaque)
{
	printf("stats_callback\n");
}

int socket_callback(int domain, int type, int protocol, void *opaque)
{
	printf("socket_callback");
}

int open_callback(const char *pathname, int flags, mode_t mode,  void *opaque)
{
	printf("open callback\n");
}
long _kafka_init(char *brokers, char mode, logCallback logger) 
{
	long handler = gen_handler();
	if(handler < 0)return handler;
	rd_kafka_t *rk;
        char errstr[512];

        rd_kafka_conf_t *conf;

	long type = mode=='P'? RD_KAFKA_PRODUCER : RD_KAFKA_CONSUMER;

        /* Kafka configuration */
        conf = rd_kafka_conf_new();
	rd_kafka_conf_set_error_cb(conf, error_callback);
	rd_kafka_conf_set_dr_cb(conf, put_callback);
	rd_kafka_conf_set_stats_cb(conf, stats_callback);
	//rd_kafka_conf_set_socket_cb(conf, socket_callback);
	rd_kafka_conf_set_open_cb(conf, open_callback);
	rd_kafka_conf_set_log_cb(conf, NULL);
	rd_kafka_conf_set_opaque(conf, (void *)(intptr_t)handler);


	/* Create Kafka handle */
	if (!(rk = rd_kafka_new(type, conf, errstr, sizeof(errstr)))) {
		logger("%% Failed to create new producer: %s\n", errstr);
		return -1;
        }

	/* Add brokers */
	if (rd_kafka_brokers_add(rk, brokers) == 0) {
		logger("%% No valid brokers specified\n");
		return -1;
	}
	if(!set_rd_kafka_t(handler, rk)){
		logger("%% Set handler to cache failed\n");
		return -1;
	}	
	return handler;
}

long _kafka_put(long handler, char* topic, char *buf, size_t len, logCallback logger)
{
	rd_kafka_t *rk = get_rd_kafka_t(handler);
	if(rk == NULL)return -1;
	if(!set_shared_msg(handler, INT_MAX, "OK")){
		_kafka_close(handler, logger);
		return -1;
	}
        long partition = 0;
        rd_kafka_topic_conf_t *topic_conf;
	rd_kafka_topic_t *rkt;
	/* Topic configuration */
	topic_conf = rd_kafka_topic_conf_new();

	/* Create topic */
	rkt = rd_kafka_topic_new(rk, topic, topic_conf);

	if (buf[len-1] == '\n')buf[--len] = '\0';

	/* Send/Produce message. */
	if (rd_kafka_produce(rkt, partition,
				RD_KAFKA_MSG_F_COPY,
				/* Payload and length */
				buf, len,
				/* Optional key and its length */
				NULL, 0,
				/* Message opaque, provided in
				 * delivery report callback as
				 * msg_opaque. */
				NULL) == -1) {
		logger("%% Failed to produce to topic %s partition %i: %s\n",
				rd_kafka_topic_name(rkt), partition,
				rd_kafka_err2str(rd_kafka_errno2err(errno)));
		/* Poll to handle delivery reports */
		rd_kafka_poll(rk, 0);
		return -1;
	}

	/* Poll to handle delivery reports */
	rd_kafka_poll(rk, 0);
	/* Wait for messages to be delivered */
	shared_msg* msg = get_shared_msg(handler);
	while (msg!=NULL && msg->err==INT_MAX && rd_kafka_outq_len(rk) > 0){
		rd_kafka_poll(rk, 10);
		msg = get_shared_msg(handler);
	}
	if(msg->err!=INT_MAX && msg->err!=0)return -1;
	return len;
}


long _kafka_start_get(long handler, char* topic, long partition, long offset, logCallback logger)
{
	rd_kafka_t *rk = get_rd_kafka_t(handler);
	if(rk == NULL)return 0;
        rd_kafka_topic_conf_t *topic_conf;
	rd_kafka_topic_t *rkt;
	/* Create topic */
	rkt = rd_kafka_topic_new(rk, topic, topic_conf);

	/* Start consuming */
	if (rd_kafka_consume_start(rkt, partition, offset) == -1){
		logger("Failed to start consuming: %s\n", rd_kafka_err2str(rd_kafka_errno2err(errno)));
		return 0;
        }
	if(!set_rd_kafka_topic_t(handler, rkt)){
		/* Stop consuming */
		rd_kafka_consume_stop(rkt, partition);
		rd_kafka_topic_destroy(rkt);
		return 0;
	}
	return 1;
}

long _kafka_get(long handler, long partition, long* offset, char **data, logCallback logger, copyCallback copy)
{
	rd_kafka_topic_t *rkt = get_rd_kafka_topic_t(handler);
	if(rkt==NULL)return -1;
	long msg_len = 0;
	int try_count = 5;
	while (1) {
		rd_kafka_message_t *rkmessage;
		/* Consume single message.
		 * See rdkafka_performance.c for high speed
		 * consuming of messages. */
		rkmessage = rd_kafka_consume(rkt, partition, 200);
		if (!rkmessage) {
			if(try_count-- < 1)return INT_MIN - 1;
			continue; /* time out */
		}
	
		if (rkmessage->err && rkmessage->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
			logger("Consumer reached end of %s [%"PRId32"] message queue at offset %"PRId64"\n",
					rd_kafka_topic_name(rkmessage->rkt),
					rkmessage->partition, rkmessage->offset);
			return INT_MIN;
                } else if (rkmessage->err) {
			logger("Consume error for topic \"%s\" [%"PRId32"] offset %"PRId64": %s\n",
					rd_kafka_topic_name(rkmessage->rkt),
					rkmessage->partition,
					rkmessage->offset,
					rd_kafka_message_errstr(rkmessage));
			return -2;
		}

		//logger("Message (offset %"PRId64", %zd bytes):\n",rkmessage->offset, rkmessage->len);
		msg_len = copy(data, rkmessage->payload, rkmessage->len);	
		*offset = rkmessage->offset;

		/* Return message to rdkafka */
		rd_kafka_message_destroy(rkmessage);
		break;
	}
	return msg_len;
}

long _kafka_end_get(long handler, long partition, logCallback logger)
{
	rd_kafka_topic_t *rkt = get_rd_kafka_topic_t(handler);
	if(rkt==NULL)return -1;
	/* Stop consuming */
	rd_kafka_consume_stop(rkt, partition);
	rd_kafka_topic_destroy(rkt);
}

void _kafka_close(long handler, logCallback logger)
{
	rd_kafka_t *rk = get_rd_kafka_t(handler);
	if(rk == NULL)return;
	release_rd_kafka_t(handler);
	release_shared_msg(handler);
	/* Destroy the handle */
	rd_kafka_destroy(rk);
}
