#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "openssl/sha.h"
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include <curl/curl.h>

#include "phoenix.h"

#define CLIENT_KEY_FILENAME "client.key"
#define CLIENT_CRT_FILENAME "client.crt"

typedef struct {
  int count; 
  char *buffer;
} curl_buffer;

static void key_generation_status(int a, int b, void *c) {
  print_info("status: a: %d, b: %d, c:0x%08x\n", a,b,c);
}

static size_t certificate_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  curl_buffer *response=(curl_buffer *)userp;

  if(response->buffer==NULL){
    debug_printf("Allocating first buffer\n");
    response->buffer=calloc(size,nmemb);
  }else{
    debug_printf("Reallocating\n");
    response->buffer=realloc(response->buffer, size * (nmemb + response->count));
  }

  memcpy(response->buffer + response->count, contents, size*nmemb);
  response->count+=size*nmemb;

  return size*nmemb;
}

int file_exists (char *filename) {
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

int phoenix_provision_device(phoenix_t *phoenix) {
  int ret,i;
  int status=0;
  char provisioning_url[1024];
  char auth_header[1024];
  unsigned char *certificate_request;
  unsigned int certificate_request_len=0;

  CURL *curl;
  CURLcode curl_code;
  struct curl_slist *list = NULL;
  long http_code;
  curl_buffer response;

  BIO *bio=BIO_new(BIO_s_mem());
  EVP_PKEY *pkey=NULL;
  RSA *rsa=NULL;
  X509_REQ *x509=NULL;
  X509_NAME *name=NULL;
  FILE *f;
  long long start = phoenix_get_timestamp();

  
  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();


  if(!phoenix->certificate && file_exists(CLIENT_CRT_FILENAME)) {
    if(load_certificate(phoenix)) {
      print_fatal("Error loading client certificate\n");
    }

    if(!verify_certificate(phoenix)) {
      print_info("Valid client certificate found, skipping provisioning\n");
      goto CLEANUP_SSL;
    }
  }

  printf("Creating key store\n");
  pkey = EVP_PKEY_new();

  if(file_exists(CLIENT_KEY_FILENAME)) {
    rsa = RSA_new();
    print_info("Opening key file\n");
    f = fopen(CLIENT_KEY_FILENAME,"rb");
    if(f==NULL){
      print_fatal("Could not open key file\n");
    }

    print_info("Reading key file\n");
    if(!PEM_read_PrivateKey(f,&pkey,NULL,NULL)) {
      print_error("Error reading private key file: %s\n", ERR_error_string(ERR_get_error(),NULL));
      ERR_print_errors_fp(stdout);
      exit(-1);
      fclose(f);
      status=-1;
      goto CLEANUP_SSL;
    }

    print_info("RSA file successfully loaded\n");
    fclose(f);
  }else{

    printf("Generating key\n");
    rsa = RSA_generate_key(
        2048,   /* number of bits for the key - 2048 is a sensible value */
        RSA_F4, /* exponent - RSA_F4 is defined as 0x10001L */
        key_generation_status,   /* callback - can be NULL if we aren't displaying progress */
        NULL    /* callback argument - not needed in this case */
        );

    printf("Storing key\n");
    EVP_PKEY_assign_RSA(pkey, rsa);
  }


  printf("Generating x509\n");
  x509 = X509_REQ_new();

  name = X509_REQ_get_subject_name(x509);

  X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,
      (unsigned char *)"DK", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,
      (unsigned char *)"ae101", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
      (unsigned char *)phoenix->device_id, -1, -1, 0);




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

  printf("Writing certificate request\n");
  f = fopen("client.csr", "wb");
  PEM_write_X509_REQ(
      f,   /* write the certificate to the file we've opened */
      x509 /* our certificate */
      );
  fclose(f);

  PEM_write_bio_X509_REQ(bio,x509);
  certificate_request_len=BIO_get_mem_data(bio,&certificate_request);
  
  //Clear response buffer
  memset(&response,0,sizeof(response));

    //Construct curl request
  curl_global_init(CURL_GLOBAL_ALL);
  curl=curl_easy_init();

  sprintf(provisioning_url,"https://%s/device/%s/certificate",phoenix->server,phoenix->device_id);
  curl_easy_setopt(curl,CURLOPT_URL,provisioning_url);
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION, certificate_callback);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,certificate_request);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,certificate_request_len);
  curl_easy_setopt(curl,CURLOPT_VERBOSE,debug);

  if(phoenix->certificate_hash != NULL) {
    sprintf(auth_header,"Authorization: Bearer %s", phoenix->certificate_hash);
    list = curl_slist_append(list, auth_header);
  }
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);  

#ifdef CLOUDGATE
  curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/cacert.pem");
#endif 
  
  curl_code=curl_easy_perform(curl);
  if(curl_code != CURLE_OK) {
    print_error("Curl error: %s\n", curl_easy_strerror(curl_code));
  }
  
  curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
  if(http_code == 200) {
    i=0;
    do {
      f = fopen(CLIENT_CRT_FILENAME,"w");
      if(f==NULL){
        print_error("Could not open client certificate file: %s\n", strerror(errno));
        sleep(1);
      } 
      if(++i >= 10) {
        print_fatal("Could not write new certificate file\n");
      }
    }while(f==NULL);

    fwrite(response.buffer, sizeof(char), response.count,f);
    fclose(f);

    //Load the new certificate
    print_info("Load the new certificate\n");
    load_certificate(phoenix);
  }else{
    print_error("Error getting new certificate: %s\n", response.buffer);
  }

  free(response.buffer);
  curl_slist_free_all(list);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

CLEANUP_SSL:
  if(x509) {
    X509_REQ_free(x509);
  }

  if(pkey){
    EVP_PKEY_free(pkey);
  }

  if(rsa){
    RSA_free(rsa);
  }

  if(bio) {
    BIO_free_all(bio);
  }

  return status;

}

int verify_certificate(phoenix_t *phoenix) {
  int ret;
  int status=-1;
  const char *ca_file="./phoenix.crt";
  BIO *bio=BIO_new(BIO_s_file());
  BIO *stdbio  = BIO_new_fp(stdout, BIO_NOCLOSE);  
  X509 *error_cert=NULL;
  X509_NAME *subject=NULL;
  X509_STORE *store=X509_STORE_new();
  X509_STORE_CTX *ctx=X509_STORE_CTX_new();

  ret = X509_STORE_load_locations(store, ca_file, NULL);
  if (ret != 1){
    print_fatal("Error loading CA cert or chain file\n");
  }

  X509_STORE_CTX_init(ctx, store, phoenix->certificate, NULL);

  ret = X509_verify_cert(ctx);
  print_info("Verification return code: %d\n", ret);

  if(ret == 0 || ret == 1){
    print_info("Verification result text: %s\n",
        X509_verify_cert_error_string(ctx->error));
  }

  if(ret == 0) {
    /*  get the offending certificate causing the failure */
    error_cert  = X509_STORE_CTX_get_current_cert(ctx);
    //subject = X509_NAME_new();
    subject = X509_get_subject_name(error_cert);
    print_error("Verification failed cert:\n");
    X509_NAME_print_ex(stdbio, subject, 0, XN_FLAG_MULTILINE);
    BIO_printf(stdbio, "\n");
    BIO_printf(stdbio, "Not after: ");
    ASN1_TIME_print(stdbio, X509_get_notAfter(phoenix->certificate));
    BIO_printf(stdbio, "\n");

    X509_NAME_free(subject);
  }else{
    status=0;
  }

  /* ---------------------------------------------------------- *
   *    * Free up all structures                                     *
   *       * ---------------------------------------------------------- */
  X509_STORE_CTX_free(ctx);
  X509_STORE_free(store);
  //Certificate has been freed, make sure it is zeroed
  phoenix->certificate=NULL;
  BIO_free_all(bio);
  BIO_free_all(stdbio);

  return status;
}

void sha256_string(char *string, int len, char outputBuffer[65])
{
  int i = 0;
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, string,len);
  SHA256_Final(hash, &sha256);
  for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
  {
    sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
  }
  outputBuffer[64] = 0;
}



int load_certificate(phoenix_t *phoenix)  {
  int der_len;
  FILE *f;
  unsigned char *der_crt=NULL;

  //Load certificate from file
  f=fopen(CLIENT_CRT_FILENAME, "rb");
  if(f==NULL){
    print_fatal("Could not open certificate file: %s -> %s\n", CLIENT_CRT_FILENAME, strerror(errno));
  }

  print_info("Certificate addr: 0x%08x\n",phoenix->certificate);
  if(phoenix->certificate) {
    print_info("Freeing certificate\n");
    X509_free(phoenix->certificate);

  }
  print_info("Loading certificate\n");
  phoenix->certificate=PEM_read_X509(f,NULL,0,NULL);
  if(phoenix->certificate==NULL){
    print_fatal("Error reading client certificate: %s\n",ERR_error_string(ERR_get_error(),NULL));
  }
  print_info("Certificate addr: 0x%08x\n",phoenix->certificate);

  der_len=i2d_X509(phoenix->certificate,&der_crt);
  if(der_len==0) {
    print_fatal("Error converting certificate to der: %s\n",ERR_error_string(ERR_get_error(),NULL));
  }

  if(!phoenix->certificate_hash) {
    phoenix->certificate_hash=calloc(sizeof(char),100);
  }else{
    memset(phoenix->certificate_hash,0,sizeof(char)*100);
  }
  sha256_string(der_crt,der_len,phoenix->certificate_hash);

  phoenix->certificate_not_after=X509_get_notAfter(phoenix->certificate); 

  free(der_crt);
  fclose(f);

  return 0;
}


