#include <stdio.h>
#include <mosquitto.h>
#include <phoenix.h>


#define DB_TABLE_DOUBLE "conf_double"



void db_command_double_response(struct mosquitto_message **response, char *conf);
