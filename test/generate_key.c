#include "../src/phoenix.h"

int debug=0;

int main(void) {
  phoenix_provision_device("http://192.168.100.104:4000", "device_provison_test");
}
