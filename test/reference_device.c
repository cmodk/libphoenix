#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#include "../src/phoenix.h"

int do_run=1;
int debug=0;

void signal_handler(int signo) {
  print_info("Caught signal\n");
  do_run=0;
}

int main(void) {
  int i=0;
  char json_msg[1024];
  double x,ref_value;
  long long runtime_ms=1000;
  long long timestamp=phoenix_get_timestamp();
  long long next_run=timestamp-(timestamp%runtime_ms);
  time_t unix_time;
  phoenix_t *phoenix = phoenix_init_with_server("127.0.0.1",1883, 0, "reference_device");
  //phoenix_t *phoenix = phoenix_init_with_server("192.168.100.104",8883, 1, "CG-KW4AK71004");
  //phoenix_t *phoenix = phoenix_init_with_server("hive.ae101.net",8883, 1, "reference_device");
  //phoenix_t *phoenix = phoenix_init_http("hive.ae101.net","reference_device");
  //phoenix_t *phoenix = phoenix_init_http("127.0.0.1:4010","reference_device");
  //phoenix_t *phoenix = phoenix_init_with_server("192.168.1.56",8883, 1, "reference_device");
  //phoenix_t *phoenix = phoenix_init_with_server("192.168.1.56",1883, 0, "reference_device");
  
  //Local development, use http
  if(phoenix->http) {
    sprintf(phoenix->http->scheme,"http");
  }

  db_init("./test");


  signal(SIGINT, signal_handler);

  //Get last know time
  next_run = db_int64_get("conf_double", "next_run");



  next_run=0;
  if (next_run == 0 ) {
    print_info("Looks like this is a new reference device, start over\n");
    timestamp = phoenix_get_timestamp();
    next_run=timestamp - (timestamp%runtime_ms); //timestamp;
  }

  print_info("Starting from timestamp %lld\n", next_run);

  printf("Connected to phoenix, sending some dummy data\n");
  while(do_run) {
    timestamp=phoenix_get_timestamp();

    if(timestamp>=next_run) {
      unix_time=timestamp/1000;

      if(!phoenix->certificate_not_after || ASN1_UTCTIME_cmp_time_t(phoenix->certificate_not_after,unix_time) < 0) {
        printf("Cerficate expired\n");
        phoenix_provision_device(phoenix);
      }


      x=next_run/1000.0;
      ref_value=cos(x/200.0) * sin(x/300);
      //print_info("Timestamp: %lld -> %lld\n",timestamp,next_run);

      phoenix_send_sample(phoenix,next_run,"test.inc",i*1.0f);
      phoenix_send_sample(phoenix,next_run,"test.ref_value",ref_value);

      //sprintf(json_msg,"{\"value\":%d}", i);
      //phoenix_send(phoenix,"/test", json_msg, strlen(json_msg));

      next_run+=runtime_ms;
      db_int64_set("conf_double", "next_run",next_run);
      phoenix_connection_handle(phoenix);
      i++;
    }else {
      do_run=1;
    }
    usleep(200);
  }

  print_info("i: %d\n",i);

  db_close();

}
