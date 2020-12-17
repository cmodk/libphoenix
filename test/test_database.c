#include <stdio.h>
#include "../src/phoenix.h"

int main(int argc, char *argv[]){
  char wanted[512];
  char *value;
  int ret,i;

  ret=db_init("./test");
  if(ret) {
    print_fatal("Could not init database\n");
  }

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

} 

