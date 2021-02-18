#include <string.h>
#include <curl/curl.h>

#include <openssl/pem.h>
#include "openssl/sha.h"
#include <openssl/err.h>

#include <phoenix.h>

void sha256_string(char *string, int len, char outputBuffer[65])
{
  int i = 0;
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, string,len);
  SHA256_Final(hash, &sha256);
  for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
  }
  outputBuffer[64] = 0;
}

phoenix_t *phoenix_init_http(unsigned char *server, unsigned char *device_id) {
  int der_len,i;
  unsigned char *der_crt=NULL;
  unsigned char crt_hash[65];
  X509 *loaded = NULL;
  phoenix_t *phoenix = (phoenix_t *)calloc(sizeof(phoenix_t),1);
  FILE *crt_file = fopen("client.crt","r");


  if(crt_file == NULL){
    print_fatal("Error opening client certificate\n");
  }

  loaded = PEM_read_X509(crt_file, NULL, NULL, NULL);
  if(loaded == NULL) {
    print_fatal("Error reading client certificate\n");
  }

  print_info("Loading certificate\n");
  der_len=i2d_X509(loaded,&der_crt);
  if(der_len==0) {
    print_fatal("Error converting certificate to der: %s\n",ERR_error_string(ERR_get_error(),NULL));
  }

  sha256_string(der_crt,der_len,crt_hash);

  phoenix->use_http=1;
  
  phoenix->device_id = (unsigned char *)calloc(sizeof(unsigned char),strlen(device_id)+1);
  sprintf(phoenix->device_id,"%s",device_id);

  phoenix->http_server = (unsigned char *)calloc(sizeof(unsigned char),strlen(server)+1);
  sprintf(phoenix->http_server,"%s",server);

  phoenix->http_token = (unsigned char *)calloc(sizeof(unsigned char),100);
  sprintf(phoenix->http_token,"%s",crt_hash);

  return phoenix;
}

int phoenix_http_post(phoenix_t *phoenix, const char *msg) {
  char url[1024];
  char auth_header[1024];
  CURL *curl;
  CURLcode curl_code;
  struct curl_slist *list = NULL;
  long response_code;

  curl_global_init(CURL_GLOBAL_ALL);
  curl=curl_easy_init();


  sprintf(auth_header,"Authorization: Bearer %s", phoenix->http_token);
  list = curl_slist_append(list, auth_header);


  sprintf(url,"https://%s/device/%s/notification",phoenix->http_server,phoenix->device_id);

  debug_printf("Posting: %s -> %s\n", url,msg);

  curl_easy_setopt(curl,CURLOPT_URL,url);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,msg);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,strlen(msg));
  curl_easy_setopt(curl,CURLOPT_VERBOSE,debug);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);  
  
  curl_code=curl_easy_perform(curl);
  if(curl_code != CURLE_OK) {
    print_error("Curl error: %s\n", curl_easy_strerror(curl_code));
  }else{
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    debug_printf("http status: %d\n", response_code);
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  curl_slist_free_all(list);

  return response_code!=200;
}

int phoenix_http_send(phoenix_t *phoenix, unsigned char *msg, int len){
  return phoenix_http_post(phoenix,msg);
}

int phoenix_http_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value){
  char ts[100];
  char msg[1024];
  
  getRFC3339(timestamp,ts);

  //Handcrafted json is nice...
  snprintf(msg,sizeof(msg), "{"
      "\"notification\":\"stream\","
      "\"parameters\": {"
      "\"timestamp\":\"%s\","
      "\"code\":\"%s\","
      "\"value\":%f"
      "}"
      "}",
      ts,stream,value);

    printf("Notification -> %s -> %s\n",phoenix->http_server,msg);


  return phoenix_http_send(phoenix,msg,strlen(msg));
}


