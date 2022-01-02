#include <phoenix.h>
int debug=1;

int main(void) {
  int i;
  phoenix_t *phoenix = phoenix_init_with_server("hive.ae101.net",8883, 1, "reference_device");


  for(i=0;i<10;i++){
    load_certificate(phoenix); 
  }

}
