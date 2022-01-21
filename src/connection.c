#include "phoenix.h"

int phoenix_connection_handle(phoenix_t *phoenix) {
  int i, num_samples,status=0;
  phoenix_sample_t *sample;
  phoenix_sample_t *samples=calloc(sizeof(phoenix_sample_t),MAX_SAMPLES_TO_SEND);

  num_samples=db_samples_read(samples, MAX_SAMPLES_TO_SEND);

  if(phoenix->http) {
    status = phoenix_http_send_samples(phoenix,samples,num_samples);
    goto cleanup;
  }else{
    for(i=0;i<num_samples;i++) {
      sample=&(samples[i]);
      print_info("Sending sample: %s -> %lld -> %f\n", sample->stream, sample->timestamp, sample->value);
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

