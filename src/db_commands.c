#include <string.h>
#include <db_commands.h>


void db_command_double_response(struct mosquitto_message **response, char *conf) {
  int i;
  double value = db_double_get(DB_TABLE_DOUBLE, conf);
  struct mosquitto_message *msg = calloc(sizeof(struct mosquitto_message),1);
  msg->payloadlen=sizeof(double)+1;
  msg->payload=calloc(msg->payloadlen,1);

  ((uint8_t *)msg->payload)[0]=DBTYPE_DOUBLE;
  memcpy(msg->payload+1, (&value), msg->payloadlen);

  for(i=0;i<sizeof(value);i++) {
    printf("0x%02x:0x%02x\n", ((uint8_t *)msg->payload)[i+1], ((*((uint64_t *)&value)) >> i*8) & 0xff);
  }

  *response=msg;
  printf("Value: %f\n", value);
}
