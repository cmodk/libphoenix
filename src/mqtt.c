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
    }

  if(level == MOSQ_LOG_ERR) {
  }
}

phoenix_t *phoenix_init(char *host,unsigned char *device_id){
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
}

void command_config_write(phoenix_t *phoenix, uint8_t *p) {
  uint8_t type = p[0];
  uint16_t conf_len= p[1] << 8 | p[2];
  uint16_t value_len = p[3] << 8 | p[4];
  char conf[conf_len+1];
  char value[value_len+1];

  snprintf(conf,conf_len+1,"%s",p+5);
  snprintf(value,value_len+1,"%s",p+5+conf_len);

  print_info("ConfigWrite(%d): %s(%d) -> %s(%d)\n", type,conf,conf_len,value,value_len);
  db_string_upsert("conf_str", conf,value);  
}

void parse_command(phoenix_t *phoenix, struct mosquitto_message *msg) {
  int i;
  uint8_t *p = (uint8_t *)msg->payload;
  uint8_t cmd_id = p[0] << 8 | p[1];
  uint8_t payload_length = p[2] << 8 | p[3];
  uint8_t payload[1024];

  memset(payload,0,sizeof(payload));

  memcpy(payload, p+4, payload_length);


  print_info("Command received(%d): id: %d, length: %d: ", msg->payloadlen,cmd_id, payload_length);
  for(i=0;i<msg->payloadlen;i++) {
    printf("0x%02x ",p[i]);
  }
  printf("\n");

  print_info("As String: %s\n", payload);

  switch(cmd_id) {
    case 2:
      command_config_write(phoenix,payload);
      break;
    case 3:
      system("reboot");
      break;
    default:
      print_error("Unknown command id: %d\n", cmd_id);
  }
}


void mosq_message_callback(struct mosquitto *mosq, void *userdata, struct mosquitto_message *msg) {
  phoenix_t *phoenix = (phoenix_t *)userdata;
  if(strcmp(phoenix->command_topic,msg->topic) == 0) {
    parse_command(phoenix,msg);
  }else{
    print_info("Unknown message received(%d): %s\n", msg->payloadlen,msg->payload);
  }
}

phoenix_t *phoenix_init_with_server(char *host, int port, int use_tls, unsigned char *device_id) {
  int ret;
  int keepalive = 60;
  bool clean_session = true;
  const char *online_status="online";
  const char *will="offline";
  int major,minor,revision; 
  phoenix_t *phoenix = (phoenix_t *)calloc(sizeof(phoenix_t),1);


  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  if(phoenix_provision_device(host,device_id)){
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
  if(ret=mosquitto_connect(phoenix->mosq, host, port, keepalive) != MOSQ_ERR_SUCCESS){
    perror("Unable to connect");
    print_fatal("Unable to connect: %d\n",ret);
  }
  print_info("Connected\n");
  int loop = mosquitto_loop_start(phoenix->mosq);
  if(loop != MOSQ_ERR_SUCCESS){
    fprintf(stderr, "Unable to start loop: %i\n", loop);
    exit(1);
  }

  phoenix_subscribe_topics(phoenix);

  phoenix->device_id = (unsigned char *)calloc(sizeof(unsigned char),strlen(device_id)+1);
  sprintf(phoenix->device_id,"%s",device_id);

  print_info("Sending online state\n");
  phoenix_send(phoenix,phoenix->status_topic,online_status,strlen(online_status));

  print_info("Connection ready\n");
  return phoenix;
}

int phoenix_send(phoenix_t *phoenix, unsigned char *topic, unsigned char *msg, int len) {
  int status;

  if(phoenix->use_http) {
    return phoenix_http_send(phoenix,msg,len);
  }
  
  status=mosquitto_publish(phoenix->mosq, NULL,topic,len,msg,1,0);
  if(status != 0) {
    print_info("Publish status: %d\n",status);
  }

  return status;
}

void dump_variable(char *desc, void *val, int len) {
  int i;
  uint8_t *buffer = (uint8_t *)val;
  if(!debug) {
    return;
  }

  debug_printf("Variable: %s(%d) -> ",desc,len);
  for(i=0;i<len;i++){
    printf("%d ", buffer[i]);
  }
  printf("\n");
}


int phoenix_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value) {
  char topic[1024];
  unsigned char msg[2048];
  int index=0;
  int i;

  if(phoenix->use_http){
    return phoenix_http_send_sample(phoenix,timestamp,stream,value);
  }

  memset(msg,0,sizeof(unsigned char) * 2048);
  debug_printf("Sending: %s -> %lld -> %f\n",stream,timestamp,value);
  
  sprintf(topic,"/device/%s/sample",phoenix->device_id);

  if(timestamp < 0) {
    timestamp = phoenix_get_timestamp();
  }

  if(debug) {
    dump_variable("timestamp",&timestamp,sizeof(timestamp));
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

  return phoenix_send(phoenix,topic,msg,index);
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
  return phoenix_send(phoenix,topic,msg,strlen(msg));
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

	char timestr[100];
	strftime(buf,100,"%Y-%m-%dT%H:%M:%S.000000Z",&nowtm);

	char miliStr[100];
	sprintf(miliStr,"%06d",(stamp%1000)*1000);
	memcpy(&(buf[20]),miliStr,6);
	//printf("Timestamp RFC: %s\n",buf);
}

