#ifndef __PHOENIX_H__
#define __PHOENIX_H__

#ifndef __ZEPHYR__
#include <pthread.h>
#include <sqlite3.h>
#include <json-c/json.h>
#include <openssl/pem.h>
#include <debug.h>
#endif

#define HTTP_QUEUE_MAX 100

typedef struct {
  char *scheme;
  char *server;
  char *token;
  
#ifndef __ZEPHYR__
  pthread_mutex_t *mutex;
#endif

  int queue_length;
  struct json_object **queue; 
} phoenix_http_t;


typedef struct {
  int connected;
  struct mosquitto *mosq;
  char *device_id;
  char status_topic[256];
  char command_topic[256];
  
  char *server;
  
  X509 *certificate;
  char *certificate_hash;
  ASN1_TIME *certificate_not_after;

  phoenix_http_t *http;
} phoenix_t; 

phoenix_t *phoenix_init(char *host, const char *device_id);
phoenix_t *phoenix_init_with_server(char *host, int port, int use_tls, const char *device_id);
phoenix_t *phoenix_init_http(unsigned char *host, const char *device_id);
int phoenix_send(phoenix_t *phoenix, const char *topic, const char *msg, int len);
int phoenix_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value);
int phoenix_send_string(phoenix_t *phoenix, long long timestamp, unsigned char *stream, char *value);


//HTTP interface
int phoenix_http_send(phoenix_t *phoenix, const char *msg, int len);
int phoenix_http_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value);


long long phoenix_get_timestamp();
void getRFC3339(long long stamp, char buf[100]);


//Provisioning
int phoenix_provision_device(phoenix_t *phoenix);
int load_certificate(phoenix_t *phoenix);
int verify_certificate(phoenix_t *phoenix);


//Database functions
typedef enum {
  DBTYPE_STRING,
  DBTYPE_INT,
  DBTYPE_DOUBLE,
  DBTYPE_INT64,
} database_type_t;

typedef enum {
  COMMAND_UNKNONW=0,
  COMMAND_CONFIG_READ,
  COMMAND_CONFIG_WRITE,
  COMMAND_CONFIG_REBOOT = 10000,
} command_type_t;


typedef struct {
  char name[256];
  database_type_t type;
  void *value;
} database_column_t;

int db_init(char *path);
int db_close();
int db_exec(char *sql);
char *db_string_get(char *table, char *key);
int db_string_upsert(char *table, char *key, char *value);

int db_double_set(char *table, char *key, double value);
double db_double_get(char *table, char *key);
int db_int64_set(char *table, char *key, int64_t value);
int64_t db_int64_get(char *table, char *key);

int db_row_ids(char *table, int **ids);
int db_row_write(char *table, database_column_t *column, int num_columns);
int db_row_read(char *table, int id, database_column_t *columns, int num_columns);

int db_sample_insert(struct json_object *sample);
int db_sample_sent(struct json_object *sample, int remove);
struct json_object *db_samples_read(int limit);

#endif // __PHOENIX_H__
