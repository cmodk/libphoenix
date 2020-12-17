#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include <curl/curl.h>

#include "phoenix.h"

#define CLIENT_KEY_FILENAME "client.key"


static void key_generation_status(int a, int b, void *c) {
  print_info("status: a: %d, b: %d, c:0x%08x\n", a,b,c);
}

static size_t certificate_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  unsigned char *response=(unsigned char *) contents;
  FILE *fp = fopen("client.crt","w");
  print_info("Response:\n %s\n",response);
  fwrite(contents,size,nmemb,fp);
  fclose(fp);

  return size*nmemb;
}

int file_exists (char *filename) {
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

int phoenix_provision_device(char *host, char *device_id) {
  int ret;
  char provisioning_url[1024];
  unsigned char *certificate_request;
  unsigned int certificate_request_len=0;
  BIO *bio=BIO_new(BIO_s_mem());
  CURL *curl;
  CURLcode curl_code;
  EVP_PKEY *pkey;
  RSA *rsa;
  X509_REQ *x509;
  X509_NAME *name;
  FILE *f;
  long long start = phoenix_get_timestamp();

  if(file_exists(CLIENT_KEY_FILENAME)) {
    print_info("Client key found, skipping provisioning\n");
    return 0;
  }
  

  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  curl_global_init(CURL_GLOBAL_ALL);
  curl=curl_easy_init();



  printf("Creating key store\n");
  pkey = EVP_PKEY_new();

  printf("Generating key\n");
  rsa = RSA_generate_key(
      2048,   /* number of bits for the key - 2048 is a sensible value */
      RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
      key_generation_status,   /* callback - can be NULL if we aren't displaying progress */
      NULL    /* callback argument - not needed in this case */
      );

  printf("Storing key\n");
  EVP_PKEY_assign_RSA(pkey, rsa);


  printf("Generating x509\n");
  x509 = X509_REQ_new();

  name = X509_REQ_get_subject_name(x509);

  X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
      (unsigned char *)"DK", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
      (unsigned char *)"ae101", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
      (unsigned char *)device_id, -1, -1, 0);




  printf("Setting pubkey for CSR\n");
  X509_REQ_set_pubkey(x509, pkey);

  printf("Creating CSR\n");
  X509_REQ_sign(x509, pkey, EVP_sha256());

  printf("Writing key to file\n");
  f = fopen(CLIENT_KEY_FILENAME, "wb");
  ret=PEM_write_PrivateKey(
      f,                  /* write the key to the file we've opened */
      pkey,               /* our key from earlier */
      NULL, /* default cipher for encrypting the key on disk */
      NULL,       /* passphrase required for decrypting the key on disk */
      0,                 /* length of the passphrase string */
      NULL,               /* callback for requesting a password */
      NULL                /* data to pass to the callback */
      );
  fclose(f);
  if(!ret) {
    print_error("Key write error: %s\n", ERR_error_string(ERR_get_error(),NULL));
    ERR_print_errors_fp(stdout);
  }

  printf("Key write result: %d\n", ret);

  printf("Writing certificate\n");
  f = fopen("client.csr", "wb");
  PEM_write_X509_REQ(
      f,   /* write the certificate to the file we've opened */
      x509 /* our certificate */
      );
  fclose(f);

  PEM_write_bio_X509_REQ(bio,x509);
  certificate_request_len=BIO_get_mem_data(bio,&certificate_request);

  sprintf(provisioning_url,"https://%s/device/%s/certificate",host,device_id);
  curl_easy_setopt(curl,CURLOPT_URL,provisioning_url);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION, certificate_callback);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,certificate_request);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,certificate_request_len);
  curl_easy_setopt(curl,CURLOPT_VERBOSE,1L);
  
  curl_code=curl_easy_perform(curl);
  if(curl_code != CURLE_OK) {
    print_error("Curl error: %s\n", curl_easy_strerror(curl_code));
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
}
