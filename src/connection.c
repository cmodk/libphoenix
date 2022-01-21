#include "phoenix.h"

int phoenix_connection_handle(phoenix_t *phoenix) {
  int i, num_samples,status=0;
  phoenix_sample_t *sample;
  phoenix_sample_t *samples=calloc(sizeof(phoenix_sample_t),MAX_SAMPLES_TO_SEND);
  long long timestamp=phoenix_get_timestamp();
  time_t unix_time=timestamp/1000;

  if(!phoenix->certificate_not_after || ASN1_UTCTIME_cmp_time_t(phoenix->certificate_not_after,unix_time) < 0) {
    printf("Cerficate expired\n");
    if(phoenix_provision_device(phoenix)) {
      print_error("Could not re provision device");
      return -1;
    }
  }

  num_samples=db_samples_read(samples, MAX_SAMPLES_TO_SEND);

  if(phoenix->http) {
    status = phoenix_http_send_samples(phoenix,samples,num_samples);
    goto cleanup;
  }else{
    for(i=0;i<num_samples;i++) {
      sample=&(samples[i]);
      phoenix_mqtt_send_sample(phoenix,sample);

    }
  }

cleanup:
  free(samples);

  return status;
}

int phoenix_next_message_id(phoenix_t *phoenix) {
  return phoenix->message_id++;  
}

