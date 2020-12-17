#include <sqlite3.h>

#include <debug.h>
typedef struct {
  int connected;
  struct mosquitto *mosq;
  char *device_id;
  char status_topic[256];
  char command_topic[256];
} phoenix_t; 

phoenix_t *phoenix_init(char *host, unsigned char *device_id);
phoenix_t *phoenix_init_with_server(char *host, int port, int use_tls, unsigned char *device_id);
int phoenix_send(phoenix_t *phoenix, unsigned char *topic, unsigned char *msg, int len);
int phoenix_send_sample(phoenix_t *phoenix, long long timestamp, unsigned char *stream, double value);
int phoenix_send_string(phoenix_t *phoenix, long long timestamp, unsigned char *stream, char *value);


long long phoenix_get_timestamp();


//Provisioning
int phoenix_provision_device(char *host, char *device_id);


//Database functions
int db_init(char *path);
char *db_string_get(char *table, char *key);
int db_string_upsert(char *table, char *key, char *value);

int db_double_set(char *table, char *key, double value);
double db_double_get(char *table, char *key);
