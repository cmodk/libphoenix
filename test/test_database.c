#include <stdio.h>
#include <stdint.h>
#include "../src/phoenix.h"
int debug=0;

void test_modbus_table_read(void) {
  database_column_t columns[]={
    {"slave_addr",  DBTYPE_INT,   NULL},
    {"data_addr",   DBTYPE_INT,   NULL},
    {"data_len",    DBTYPE_INT,   NULL},
    {"data_type",   DBTYPE_STRING,NULL}
  };

  int *ids;
  int i,row,num_rows=0,id;
  int num_columns = sizeof(columns)/sizeof(database_column_t);
  int num_ids = db_row_ids("modbus_mapping",&ids);

  print_info("Got %d ids from modbus mappings\n", num_ids);
  for(row=0;row<num_ids;row++){
    id = ids[row];
    printf("Reading id %d\n",id);
    num_rows = db_row_read("modbus_mapping",id,columns,num_columns);
    if(num_rows == 1) {

      for(i=0;i<num_columns;i++){
        printf("%s -> 0x%08x\n", columns[i].name,columns[i].value);
      }

      printf("Slave addr: %d, data addr: %d, data_len: %d, date_type: %s\n", 
          *(int *)columns[0].value,
          *(int *)columns[1].value,
          *(int *)columns[2].value,
          (char *)columns[3].value);
    }else{
      printf("1 row was expected, %d was returned\n",num_rows);
    }

    for(i=0;i<num_columns;i++)
      free(columns[i].value);
  }

  free(ids);
}

void modbus_table_write(uint8_t slave_addr, uint16_t data_addr, uint8_t data_len, char *data_type) {
  database_column_t columns[]={
    {"slave_addr",  DBTYPE_INT,   &slave_addr},
    {"data_addr",   DBTYPE_INT,   &data_addr},
    {"data_len",    DBTYPE_INT,   &data_len},
    {"data_type",   DBTYPE_STRING,data_type}
  };
  
  int num_columns = sizeof(columns)/sizeof(database_column_t);

  return db_row_write("modbus_mapping", columns,num_columns);

}

void test_modbus_table_write(void) {
  printf("Deleting all rows in modbus mappings\n");
  db_exec("DELETE FROM modbus_mapping");

  modbus_table_write(87,4,2,"float");
  modbus_table_write(87,8,2,"float");
  modbus_table_write(87,12,2,"float");
  modbus_table_write(87,16,2,"float");
  modbus_table_write(87,20,2,"float");
  modbus_table_write(87,24,2,"float");
  modbus_table_write(87,28,2,"float");

  printf("Data inserted\n");
}

int main(int argc, char *argv[]){
  char wanted[512];
  char *value;
  int ret,i;

  ret=db_init("./test");
  if(ret) {
    print_fatal("Could not init database\n");
  }

  test_modbus_table_write();
  test_modbus_table_read();

  value=db_string_get("conf_str","test_string");
  print_info("Get test_string: %s\n", value);
  free(value);
  for(i=0;i<100;i++) {
    sprintf(wanted,"test_value_%d", i);
    db_string_upsert("conf_str", "test_string", wanted);

    value=db_string_get("conf_str","test_string");
    print_info("Get test_string: %s\n", value);
    free(value);
  }

  //Should be NULL, when reading unknown string
  value=db_string_get("conf_str","unknown");
  print_info("Value unknown: 0x%08x\n", value);

  db_close();

} 



