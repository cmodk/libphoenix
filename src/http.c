#include <string.h>
#include <curl/curl.h>

#include <phoenix.h>

typedef struct {
  size_t size;
  char *data;
} http_response_t;

void command_db_write(json_object *parameters) {
  int i,num_columns,*ival;
  json_object *table, *columns;
  database_column_t *column_data=NULL;


  if(!json_object_object_get_ex(parameters,"table",&table)) {
    print_error("Missing table from command\n");
    return;
  }

  if(!json_object_object_get_ex(parameters,"columns",&columns)) {
    print_error("Missing columns from command\n");
    return;
  }


  i=0;
  json_object_object_foreach(columns,key,column_value) {
    column_data = realloc(column_data,sizeof(database_column_t)*(i+1));
    printf("col(%d): %s\n", i+1, key);
    sprintf(column_data[i].name,"%s",key);
    switch(json_object_get_type(column_value)) {
      case json_type_int:
        column_data[i].type=DBTYPE_INT;
        ival=malloc(sizeof(int));
        *ival=json_object_get_int(column_value);
        column_data[i].value=(void *)ival;
        printf("\tinteger: %d\n", *ival);
        break;
      case json_type_string:
        column_data[i].type=DBTYPE_STRING;
        column_data[i].value=json_object_get_string(column_value);
        printf("\tstring: %s\n", column_data[i].value);
        break;
      case json_type_double:
        column_data[i].type=DBTYPE_DOUBLE;
        column_data[i].value=malloc(sizeof(double));
        *(double *)column_data[i].value=json_object_get_double(column_value);
        printf("\tdouble: %f\n", *(double *)column_data[i].value);
        break;
      case json_type_null:
        column_data[i].value=NULL;
        printf("\tnull\n");
        break;
      default:
        print_fatal("Unhandled json type: %s\n", json_object_get_type(column_value));
    }
    i++;
  }
  num_columns=i;

  db_row_write(json_object_get_string(table),column_data,num_columns);

  for(i=0;i<num_columns;i++) {
    if(column_data[i].type != DBTYPE_STRING)
      free(column_data[i].value);
  }
  free(column_data);
}

void command_db_read(json_object *parameters) {
  int *ids;
  int num_ids = db_row_ids("modbus_mapping",&ids);

}



void check_pending_commands(http_response_t *body) {
  int i;
  char *key;
  char *cmd;
  json_object *response=json_tokener_parse(body->data);
  json_object *pending_commands;
  json_object *command;
  json_object *command_id;
  json_object *parameters;

  debug_printf("Checking commands: %s\n", body->data);

  if(response == NULL) {
    print_error("Error parsing as json: %s\n", body->data);
    return;
  }

  pending_commands=json_object_object_get(response,"pending_commands"); 
  if(pending_commands != NULL ){
    for(i=0;i<json_object_array_length(pending_commands);i++) {
      command = json_object_array_get_idx(pending_commands,i);

      if(json_object_object_get_ex(command,"command", &command_id)){
        cmd=json_object_get_string(command_id);
        
        //Check for parameters
        if(!json_object_object_get_ex(command,"parameters",&parameters)) {
          parameters=NULL;
        }

        if(strcmp(cmd,"db_write")==0) {
          command_db_write(parameters);
        } else if(strcmp(cmd,"db_read")) {
          command_db_read(parameters);
        }else{
          print_error("Unknown command: %s\n", cmd);
        }
      }
    }
  }

  json_object_put(response);

}

phoenix_t *phoenix_init_http(unsigned char *server, const char *device_id) {
  phoenix_t *phoenix = (phoenix_t *)calloc(sizeof(phoenix_t),1);


  phoenix->http=calloc(sizeof(phoenix_http_t),1);
  
  phoenix->device_id = (char *)calloc(sizeof(char),strlen(device_id)+1);
  sprintf(phoenix->device_id,"%s",device_id);

  phoenix->http->scheme = (char *)calloc(sizeof(char),strlen("https")+1);
  sprintf(phoenix->http->scheme,"https");

  phoenix->server = (char *)calloc(sizeof(char),strlen(server)+1);
  sprintf(phoenix->server,"%s",server);

  if(phoenix_provision_device(phoenix)){
    print_fatal("Provisioning failed\n");
  }

  return phoenix;


}

size_t http_post_writer(void *data, size_t size, size_t nmemb, void *userp){
  size_t realsize = size * nmemb;
  http_response_t *mem = (http_response_t *)userp;

  char *ptr = realloc(mem->data, mem->size + realsize + 1);
  if(ptr == NULL)
    return 0;  /* out of memory! */

  mem->data = ptr;
  memcpy(&(mem->data[mem->size]), data, realsize);
  mem->size += realsize;
  mem->data[mem->size] = 0;

  return realsize;
}

int phoenix_http_post(phoenix_t *phoenix, const char *msg) {
  char url[1024];
  char auth_header[1024];
  http_response_t body;
  CURL *curl;
  CURLcode curl_code;
  struct curl_slist *list = NULL;
  long response_code;

  memset(&body,0,sizeof(body));

  curl_global_init(CURL_GLOBAL_ALL);
  curl=curl_easy_init();


  sprintf(auth_header,"Authorization: Bearer %s", phoenix->certificate_hash);
  list = curl_slist_append(list, auth_header);
  list = curl_slist_append(list, "Content-Type: application/json");
  list = curl_slist_append(list, "Expect:");


  sprintf(url,"%s://%s/device/%s/notification",phoenix->http->scheme,phoenix->server,phoenix->device_id);

  debug_printf("Posting: %s -> %s\n", url,msg);

  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,msg);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,strlen(msg));
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,http_post_writer);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA,&body);
  curl_easy_setopt(curl,CURLOPT_VERBOSE,debug);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);  
#ifdef CLOUDGATE
  curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/cacert.pem");
#endif 
  
  curl_code=curl_easy_perform(curl);
  if(curl_code != CURLE_OK) {
    print_error("Curl error: %s\n", curl_easy_strerror(curl_code));
  }else{
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    debug_printf("http status: %ld\n", response_code);
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  curl_slist_free_all(list);

  if(response_code == 200) {
    check_pending_commands(&body);
  }

  if(body.data != NULL) {
    free(body.data);
  }

  return response_code!=200;
}

int phoenix_http_send(phoenix_t *phoenix, const char *msg, int len){
  return phoenix_http_post(phoenix,msg);
}

struct json_object *phoenix_notification_init(char *notification) {
  struct json_object *n=json_object_new_object();
  struct json_object *p=json_object_new_array();

  json_object_object_add(n, "notification", json_object_new_string(notification));
  json_object_object_add(n,"parameters", p);

  return n;
}

struct json_object *copy_json_object(struct json_object *src) {
  const char *json_str=json_object_to_json_string(src);

  if(json_str == NULL){
    return NULL;
  }

  return json_tokener_parse(json_str);
}

int phoenix_http_send_samples(phoenix_t *phoenix, phoenix_sample_t *samples, int num_samples){
  char ts[100];
  int msg_len,i;
  int status=0;
  char *json_str;
  struct json_object *notification, *parameters;  
  struct json_object *sample;
  
  //Get compatible timestamp in string format

  
  notification = phoenix_notification_init("streams");
  if(!json_object_object_get_ex(notification,"parameters", &parameters)) {
    print_error("Missing parameters for notification\n");
  }

  for(i=0;i<num_samples;i++) {
    sample = json_object_new_object();
  
    getRFC3339(samples[i].timestamp,ts);

    json_object_object_add(sample, "code", json_object_new_string(samples[i].stream));
    json_object_object_add(sample, "timestamp", json_object_new_string(ts));
    json_object_object_add(sample, "value", json_object_new_double(samples[i].value));


    json_object_array_add(parameters,sample);
  }

  json_str = json_object_to_json_string_ext(notification,JSON_C_TO_STRING_PLAIN);

  if(!phoenix_http_send(phoenix,json_str,strlen(json_str))){
    //Delivery successfull. Clear the queue
    for(i=0;i<num_samples;i++) {
      //Entries from database, has an id, which need to be marked as sent
      db_sample_sent(samples[i].id,1);
    }
  }
  json_object_put(notification);



  return status;
}


