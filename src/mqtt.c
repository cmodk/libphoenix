#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <phoenix.h>
#include "version.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/time.h>

#include <debug.h>
#include <db_commands.h>

#define INSECURE_TLS 0

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    switch(level){
      case MOSQ_LOG_DEBUG:
        debug_printf("MOSQ: %i:%s\n",level,str);
        break;
      case MOSQ_LOG_INFO:
        print_info("MOSQ: %i:%s\n",level,str);
        break;
      case MOSQ_LOG_NOTICE:
        print_info("MOSQ: %i:%s\n",level,str);
        break;
      case MOSQ_LOG_WARNING:
        print_warning("MOSQ: %i:%s\n",level,str);
        break;
      case MOSQ_LOG_ERR:
        print_error("%i:%s\n", level, str);
        break;
      default:
        print_error("Unhandled level:%i:%s\n",level,str);
        break;
    }

  if(level == MOSQ_LOG_ERR) {
  }
}

phoenix_t *phoenix_init(char *host,const char *device_id){
  return phoenix_init_with_server(host,8883,1,device_id);
}

void phoenix_subscribe_topics(phoenix_t *phoenix) {
  mosquitto_subscribe(phoenix->mosq,NULL,phoenix->command_topic,2);
}

void mosq_disconnect_callback(struct mosquitto *mosq, void *userdata, int reason) {
  phoenix_t *phoenix = (phoenix_t *)userdata;
  print_info("Mosquitto disconnected: %s\n", phoenix->status_topic);
  phoenix->connected=0;
} 

void mosq_connect_callback(struct mosquitto *mosq, void *userdata, int reason) {
  phoenix_t *phoenix = (phoenix_t *)userdata;
  print_info("Mosquitto connected: %s\n", phoenix->status_topic);
  phoenix->connected=1;

  phoenix_subscribe_topics(phoenix);
} 

void mosq_publish_callback(struct mosquitto *mosq, void *userdata, int mid) {
  phoenix_t *phoenix = (phoenix_t *)userdata;
  if(mid > 0){
    
    pthread_mutex_lock(&(phoenix->connection_mutex));
    debug_printf("MID received by server: %d\n",mid);
    db_sample_sent_by_message_id(mid,0);
    phoenix->messages_in_flight--;
    pthread_mutex_unlock(&(phoenix->connection_mutex));
  }
}

char *database_type_to_string(database_type_t type) {
  printf("database type to string: %d\n",type);
  switch(type) {
    case DBTYPE_STRING:
      return "string";
      break;
    case DBTYPE_INT:
      return "int";
      break;
    case DBTYPE_DOUBLE:
      return "double";
      break;
    default:
      print_error("Unknown command type: %d\n", type);
      return "Unknown";
  }
}

void command_config_write(phoenix_t *phoenix, uint8_t *p, struct mosquitto_message **response) {
  uint8_t type = p[0];
  uint16_t conf_len= p[1] << 8 | p[2];
  uint16_t value_len = p[3] << 8 | p[4];
  void *value_position=p + 5 + conf_len;
  char conf[conf_len+1];
  char value[value_len+1];
  double fvalue;

  snprintf(conf,conf_len+1,"%s",p+5);

  print_info("ConfigWrite(%d): %s(%d) -> %s(%d)\n", type,conf,conf_len,value,value_len);
  switch(type) {
    case DBTYPE_STRING:
      snprintf(value,value_len+1,"%s",value_position);
      db_string_upsert("conf_str", conf,value);  
      break;
    case DBTYPE_DOUBLE:
      if(value_len != sizeof(double)){
        print_error("Wrong size for double write: %d != %d\n", value_len, sizeof(double));
        return;
      }
      memcpy(&fvalue, value_position, value_len);
      db_double_set("conf_double", conf, fvalue);
      break;
    default:
      print_error("Unknown type for config write: %d\n", type);
      return;
  }
}

void command_config_read(phoenix_t *phoenix, uint8_t *p, struct mosquitto_message **response) {
  command_type_t cmd_type = p[0];
  uint16_t conf_len= p[1] << 8 | p[2];
  char conf[conf_len+1];

  printf("conf len: %d\n", conf_len);
  snprintf(conf,conf_len+1,"%s",p+3);

  print_info("ConfigRead(%d->%s): %s(%d)\n", cmd_type,database_type_to_string(cmd_type), conf,conf_len);

  switch(cmd_type) {
    case DBTYPE_DOUBLE:
      db_command_double_response(response,conf);
      break;
    default:
      print_error("Unknown command type: %d\n", cmd_type);
  }
}




void parse_command(phoenix_t *phoenix, const struct mosquitto_message *msg) {
  int i;
  struct mosquitto_message *response=NULL;
  uint8_t *p = (uint8_t *)msg->payload;
  uint64_t id = (
      (uint64_t)p[0] << 56 | 
      (uint64_t)p[1] << 48 | 
      (uint64_t)p[2] << 40 | 
      (uint64_t)p[3] << 32 | 
      (uint64_t)p[4] << 24 | 
      (uint64_t)p[5] << 16 | 
      (uint64_t)p[6] << 8 | 
      (uint64_t)p[7] << 0);
  command_type_t cmd = p[8] << 8 | p[9];
  uint8_t payload_length = p[10] << 8 | p[11];
  uint8_t payload[1024];
  unsigned char response_topic[1024];

  memset(payload,0,sizeof(payload));

  memcpy(payload, p+12, payload_length);


  print_info("Command received(%d): id: %lld, cmd: %d, length: %d: ", msg->payloadlen,id, cmd, payload_length);
  for(i=0;i<msg->payloadlen;i++) {
    printf("0x%02x ",p[i]);
  }
  printf("\n");

  switch(cmd) {
    case COMMAND_CONFIG_READ:
      command_config_read(phoenix,payload, &response);
      break;
    case COMMAND_CONFIG_WRITE:
      command_config_write(phoenix,payload, &response);
      break;
    case COMMAND_CONFIG_REBOOT:
      system("reboot");
      break;
    default:
      print_error("Unknown command id: %d\n", cmd);
  }

  if(response != NULL) {
    sprintf(response_topic, "/device/%s/command/%llu", phoenix->device_id, id);
    print_info("Sending response to %s, %d bytes\n", response_topic,response->payloadlen);
    phoenix_mqtt_send(phoenix,NULL,response_topic, response->payload, response->payloadlen);
    free(response);
  }
    
}


void mosq_message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
  phoenix_t *phoenix = (phoenix_t *)userdata;
  if(strcmp(phoenix->command_topic,msg->topic) == 0) {
    parse_command(phoenix,msg);
  }else{
    print_info("Unknown message received(%d): %s\n", msg->payloadlen,(const char *)msg->payload);
  }
}

static void *connection_handler(void *input) {
  phoenix_t *phoenix = (phoenix_t *)input;

  //Wait for database
  while(!db_ready()){
    print_info("Waiting for database\n");
    sleep(1);
  }


  while(phoenix->mosq) {
    phoenix_connection_handle(phoenix);

    sleep(1);
  }

  print_info("Connection thread ended\n");
}


phoenix_t *phoenix_init_with_server(char *host, int port, int use_tls, const char *device_id) {
  int ret;
  int keepalive = 60;
  bool clean_session = true;
  const char *online_status="online";
  const char *will="offline";
  int major,minor,revision; 
  phoenix_t *phoenix = (phoenix_t *)calloc(1,sizeof(phoenix_t));

  print_info("Certificate addr: 0x%08x\n",phoenix->certificate);

  phoenix->server = (char *)calloc(sizeof(char),strlen(host)+1);
  sprintf(phoenix->server,"%s",host);

  phoenix->device_id = (char *)calloc(sizeof(char),strlen(device_id)+1);
  sprintf(phoenix->device_id,"%s",device_id);

  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  print_info("Certificate addr: 0x%08x\n",phoenix->certificate);
  if(phoenix_provision_device(phoenix)){
    print_fatal("Provisioning failed\n");
  }

  mosquitto_lib_init();
  mosquitto_lib_version(&major,&minor,&revision);

  print_info("phoenix: %s\n",VERSION);
  print_info("Initializating mosquitto: %d.%d.%d\n",major,minor,revision);
  phoenix->mosq = mosquitto_new(device_id, clean_session, phoenix);
  if(!phoenix->mosq){
    fprintf(stderr, "Error: Out of memory.\n");
    exit(1);
  }
  
  print_info("Adding will\n");
  sprintf(phoenix->status_topic, "/device/%s/status", device_id);
  sprintf(phoenix->command_topic, "/device/%s/command", device_id);

  mosquitto_log_callback_set(phoenix->mosq, mosq_log_callback);
  mosquitto_connect_callback_set(phoenix->mosq, mosq_connect_callback);
  mosquitto_disconnect_callback_set(phoenix->mosq, mosq_disconnect_callback);
  mosquitto_publish_callback_set(phoenix->mosq,mosq_publish_callback);
  mosquitto_message_callback_set(phoenix->mosq, mosq_message_callback);
  mosquitto_int_option(phoenix->mosq,MOSQ_OPT_PROTOCOL_VERSION,MQTT_PROTOCOL_V5);

  if(mosquitto_will_set(phoenix->mosq, phoenix->status_topic, strlen(will),will,1,1) != MOSQ_ERR_SUCCESS) {
    print_fatal("Could not set up will\n");
  }
  if(use_tls) {
    print_info("Setting up tls\n");
    ret=mosquitto_tls_set(phoenix->mosq,
        "phoenix.crt",
        NULL,
        "client.crt",
        "client.key",
        NULL);
    if(ret != MOSQ_ERR_SUCCESS) {
      fprintf(stderr,"Could not set tls context: %s\n",mosquitto_strerror(ret));
      exit(-1);
    }


#if INSECURE_TLS==1
    print_error("RUNNING WITH INSECURE TLS SETTINGS\n");
    mosquitto_tls_opts_set(phoenix->mosq,0, "tlsv1.2",NULL);
    mosquitto_tls_insecure_set(phoenix->mosq, 1);
#endif
  }




  print_info("Connecting to server: %s:%d\n",host,port);
  if( (ret=mosquitto_connect(phoenix->mosq, host, port, keepalive)) != MOSQ_ERR_SUCCESS){
    perror("Unable to connect");
    print_error("Unable to connect: %d\n",ret);
  }
  print_info("Connected\n");
  int loop = mosquitto_loop_start(phoenix->mosq);
  if(loop != MOSQ_ERR_SUCCESS){
    fprintf(stderr, "Unable to start loop: %i\n", loop);
    exit(1);
  }

  phoenix_subscribe_topics(phoenix);

  phoenix->device_id = (char *)calloc(sizeof(unsigned char),strlen((const char *)device_id)+1);
  sprintf(phoenix->device_id,"%s",device_id);

  print_info("Sending online state\n");
  phoenix_mqtt_send(phoenix,NULL,phoenix->status_topic,online_status,strlen(online_status));


  pthread_mutex_init(&(phoenix->connection_mutex),NULL);
  pthread_create(&(phoenix->connection_thread), NULL, connection_handler, phoenix);

  print_info("Connection ready\n");
  return phoenix;
}

void phoenix_close(phoenix_t *phoenix) {
  mosquitto_destroy(phoenix->mosq);

  phoenix->mosq=NULL;
  pthread_join(phoenix->connection_thread,NULL);
}

int phoenix_mqtt_send(phoenix_t *phoenix, int *mid, const char *topic, const char *msg, int len) {
  int status;

  if(phoenix->http) {
    return phoenix_http_send(phoenix,msg,len);
  }
  
  pthread_mutex_lock(&(phoenix->connection_mutex));
  status=mosquitto_publish(phoenix->mosq, mid,topic,len,msg,1,2);
  if(status != 0) {
    print_info("Publish status: %d\n",status);
  }
  phoenix->messages_in_flight++;
  pthread_mutex_unlock(&(phoenix->connection_mutex));

  return status;
}

int phoenix_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value) {
  return db_sample_insert(stream,timestamp,value);
}

int phoenix_mqtt_send_sample(phoenix_t *phoenix, phoenix_sample_t *sample) {
  char topic[1024];
  char msg[2048];
  int index=0;
  int i;
  int status=0;
  int mid;
  char *stream=sample->stream;
  long long timestamp=sample->timestamp;
  double value=sample->value;

  memset(msg,0,sizeof(unsigned char) * 2048);
  debug_printf("Sending: %s -> %lld -> %f\n",stream,timestamp,value);
  
  sprintf(topic,"/device/%s/sample",phoenix->device_id);

  if(timestamp < 0) {
    timestamp = phoenix_get_timestamp();
  }

  memcpy(&(msg[index]),&timestamp,sizeof(timestamp));
  index+=sizeof(timestamp);
  
  memcpy(&(msg[index]),&value,sizeof(value));
  index+=sizeof(value);

  index+=sprintf(&(msg[index]),"%s",stream);

  if(debug) {
    debug_printf("Sending sample message(%d): ",index);
    for(i=0;i<index;i++){
      printf("%d ",msg[i]);
    }
    printf("\n");
  }

  //Save the message id for the sample

  if(phoenix_mqtt_send(phoenix,&mid,topic,msg,index)) {
    print_error("Could not publish sample\n");
  }
  
  debug_printf("Sample %d has mid %d\n", sample->id, mid);
  return db_sample_set_message_id(sample->id, mid);
  
}

int phoenix_send_string(phoenix_t *phoenix, long long timestamp, unsigned char *stream, char *value) {
  char topic[1024];
  char msg[1024];
  char ts[100];

  sprintf(topic,"/device/%s/notification", phoenix->device_id);

  getRFC3339(timestamp,ts);
  //Handcrafted json is nice...
  snprintf(msg,sizeof(msg), "{"
      "\"notification\":\"stream\","
      "\"parameters\": {"
      "\"timestamp\":\"%s\","
      "\"code\":\"%s\","
      "\"value\":\"%s\""
      "}"
      "}",
      ts,stream,value);

    printf("%s -> %s\n",topic,msg);
  return phoenix_mqtt_send(phoenix,NULL, topic,msg,strlen(msg));
}

//Return unix time in milli seconds
long long phoenix_get_timestamp(){
  struct timeval tv;
  long long timestamp; 

  if(gettimeofday(&tv,NULL)){
    perror("ERROR getting timestamp, not possible.. exiting..");
    exit(-1);
  }

  timestamp = tv.tv_sec * 1e3 + (tv.tv_usec / 1e3);

  return timestamp;
}

void getRFC3339(long long stamp, char buf[100])
{
	struct tm nowtm;
	time_t nowtime;

	nowtime=stamp/1000;
	gmtime_r(&nowtime,&nowtm);

	strftime(buf,100,"%Y-%m-%dT%H:%M:%S.000000Z",&nowtm);

	char miliStr[100];
	sprintf(miliStr,"%06lld",(stamp%1000)*1000);
	memcpy(&(buf[20]),miliStr,6);
	//printf("Timestamp RFC: %s\n",buf);
}

