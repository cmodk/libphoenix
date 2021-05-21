#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <linux/limits.h>
#include <math.h>
#include <phoenix.h>

#define SAMPLES_INSERT_STMT "INSERT INTO samples VALUES(NULL,?,?,?,0);"
#define SAMPLES_READ_STMT "SELECT id,code,timestamp,value FROM samples WHERE is_sent=? LIMIT ?;"
#define SAMPLES_IS_SENT_STMT "UPDATE samples SET is_sent=1 WHERE id = ?;"
#define SAMPLES_DELETE_STMT "DELETE FROM samples WHERE id = ?;"


static sqlite3 *db;
static char workpath[PATH_MAX-1];
static pthread_mutex_t db_mutex;
static sqlite3_stmt *db_sample_insert_stmt;
static sqlite3_stmt *db_samples_read_stmt;
static sqlite3_stmt *db_sample_is_sent_stmt;
static sqlite3_stmt *db_sample_delete_stmt;

int db_init(char *path) {
  int ret; 
  char** pStatement=NULL;
  char *zErrMsg = NULL;
  char dbpath[PATH_MAX-1];


  sprintf(workpath,"%s",path);
  sprintf(dbpath,"%s/phoenix.db",workpath);

  if( ret=sqlite3_open(dbpath, &db)) {
    print_error("Can't open database: %s -> %s\n", dbpath, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  if(ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS conf_str(id INTEGER PRIMARY KEY AUTOINCREMENT, key STRING NOT NULL UNIQUE, value STRING);",NULL,0,zErrMsg) != SQLITE_OK)  {
    print_error("Could not create string table: %d -> %s -> %s \n",ret, zErrMsg, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  if(ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS conf_double(id INTEGER PRIMARY KEY AUTOINCREMENT, key STRING NOT NULL UNIQUE, value DOUBLE);",NULL,0,zErrMsg) != SQLITE_OK)  {
    print_error("Could not create string table: %d -> %s -> %s \n",ret, zErrMsg, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  if(ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS samples(id INTEGER PRIMARY KEY AUTOINCREMENT, code STRING NOT NULL, timestamp STRING NOT NULL, value DOUBLE, is_sent INT DEFAULT 0);",NULL,0,zErrMsg) != SQLITE_OK)  {
    print_error("Could not create sample table: %d -> %s -> %s \n",ret, zErrMsg, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  if(sqlite3_prepare(db,SAMPLES_INSERT_STMT,strlen(SAMPLES_INSERT_STMT), &db_sample_insert_stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  if(sqlite3_prepare(db,SAMPLES_READ_STMT,strlen(SAMPLES_READ_STMT), &db_samples_read_stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  if(sqlite3_prepare(db,SAMPLES_IS_SENT_STMT,strlen(SAMPLES_IS_SENT_STMT), &db_sample_is_sent_stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  if(sqlite3_prepare(db,SAMPLES_DELETE_STMT,strlen(SAMPLES_DELETE_STMT), &db_sample_delete_stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing delete statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  pthread_mutex_init(&db_mutex,NULL);

  return 0;
}

int db_close() {
  sqlite3_close(db);
}

int db_exec(char *sql) {
  int ret; 
  char *zErrMsg = NULL;

  printf("DB: Executing: %s\n", sql);
  if( ret=sqlite3_exec(db,sql,NULL,0,zErrMsg)) {
    print_error("Error executing '%s' -> %s\n", sql, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  return 0;
}

char *db_string_get(char *table, char *key) {
  int id;
  char *value=NULL;
  sqlite3_stmt* stmt;
  char *str;
  char sql[512];
  
  sprintf(sql,"SELECT id,value FROM %s WHERE key = ?",table);
  
  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    id = sqlite3_column_int(stmt, 0);
    str=sqlite3_column_text(stmt,1);
    value=(char *)calloc(sizeof(char),strlen(str)+1);
    sprintf(value,"%s", sqlite3_column_text(stmt, 1));
  }


  sqlite3_finalize(stmt);
  return value;
}


int db_string_upsert(char *table, char *key, char *value) {
  int ret;
  char sql[512];
  sqlite3_stmt* stmt;

  sprintf(sql,"INSERT INTO %s VALUES(NULL,?,?);", table);

  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);
  sqlite3_bind_text(stmt, 2, value, strlen(value),NULL);

  while (ret=sqlite3_step(stmt) == SQLITE_ROW)
  {
  print_info("ret: %d\n", ret);
    int id = sqlite3_column_int(stmt, 0);
    unsigned char* name = sqlite3_column_text(stmt, 1);
    printf("%d, %s\n", id, name);
  }
  sqlite3_finalize(stmt);

  if(ret != SQLITE_DONE) {
    //Insert failed, try to update
    sprintf(sql,"UPDATE %s SET value=? WHERE key=?;", table);

    if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
      print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
      return NULL;
    }

    sqlite3_bind_text(stmt, 1, value, strlen(value),NULL);
    sqlite3_bind_text(stmt, 2, key, strlen(key),NULL);

    while (ret=sqlite3_step(stmt) == SQLITE_ROW)
    {
      print_info("ret: %d\n", ret);
      int id = sqlite3_column_int(stmt, 0);
      unsigned char* name = sqlite3_column_text(stmt, 1);
      printf("%d, %s\n", id, name);
    }
    sqlite3_finalize(stmt);

  }

  return 0;
}

double db_double_get(char *table, char *key) {
  int id;
  double value=NAN;
  sqlite3_stmt* stmt;
  char *str;
  char sql[512];
  
  sprintf(sql,"SELECT id,value FROM %s WHERE key = ?",table);
  
  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NAN;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    id = sqlite3_column_int(stmt, 0);
    value=sqlite3_column_double(stmt,1);
  }


  sqlite3_finalize(stmt);
  return value;
}

int db_bind_value(sqlite3_stmt *stmt, int index, void *value, database_type_t value_type) {
  switch(value_type) {
      case DBTYPE_DOUBLE:
        sqlite3_bind_double(stmt, index, *((double *)value));
        break;
      case DBTYPE_INT64:
        sqlite3_bind_int64(stmt, index, *((int64_t *)value));
        break;
      default:
        print_error("Unhandled database type: %d\n", value_type);
        return -1;

    }

  return 0;
}

int db_value_set(char *table, char *key, void *value, database_type_t value_type) {
  int ret;
  char sql[512];
  sqlite3_stmt* stmt;

  sprintf(sql,"INSERT INTO %s VALUES(NULL,?,?);", table);

  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);
  db_bind_value(stmt,2,value,value_type);

  while (ret=sqlite3_step(stmt) == SQLITE_ROW)
  {
  print_info("ret: %d\n", ret);
    int id = sqlite3_column_int(stmt, 0);
    unsigned char* name = sqlite3_column_text(stmt, 1);
    printf("%d, %s\n", id, name);
  }
  sqlite3_finalize(stmt);

  if(ret != SQLITE_DONE) {
    //Insert failed, try to update
    sprintf(sql,"UPDATE %s SET value=? WHERE key=?;", table);

    if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
      print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
      return NULL;
    }
    
    db_bind_value(stmt,1,value,value_type);
    sqlite3_bind_text(stmt, 2, key, strlen(key),NULL);

    while (ret=sqlite3_step(stmt) == SQLITE_ROW)
    {
      print_info("ret: %d\n", ret);
      int id = sqlite3_column_int(stmt, 0);
      unsigned char* name = sqlite3_column_text(stmt, 1);
      printf("%d, %s\n", id, name);
    }
    sqlite3_finalize(stmt);

  }

  return 0;
}

int db_double_set(char *table, char *key, double value) {
  return db_value_set(table, key, &value, DBTYPE_DOUBLE);
}

int db_int64_set(char *table, char *key, int64_t value) {
  return db_value_set(table, key, &value, DBTYPE_INT64);
}

int db_row_ids(char *table, int **ids) {
  sqlite3_stmt* stmt;
  char sql[512];
  int num_ids;

  debug_printf("Getting ids from %s\n", table);
  
  sprintf(sql,"SELECT id FROM %s",table);
  
  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  num_ids=0;
  *ids=NULL;
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    *ids=realloc(*ids, sizeof(int) * (num_ids+1));
    (*ids)[num_ids++]=sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);

  return num_ids;
}

int db_row_read(char *table, int id, database_column_t *columns, int num_columns){
  int i,num_rows;
  char sql[1024];
  char keys[1024];
  const char *text;
  sqlite3_stmt* stmt;

  memset(keys,0,sizeof(keys));

  for(i=0;i<num_columns;i++) {
    if(i!=0) {
      strcat(keys,",");
    }
    strcat(keys,columns[i].name);
  }

  sprintf(sql,"select %s from %s where id = %d;", keys,table,id);

  debug_printf("SQL: %s\n",sql);

  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  num_rows=0;
  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    num_rows++;
    printf("Step\n");
    for(i=0;i<num_columns;i++) {
      if(sqlite3_column_type(stmt,i)==SQLITE_NULL) {
        columns[i].value=NULL;
        continue;
      }
      switch(columns[i].type){
        case DBTYPE_INT:
          columns[i].value = malloc(sizeof(int));
          *(int *)(columns[i].value) = sqlite3_column_int(stmt,i);
          break;
        case DBTYPE_STRING:
          text = (const char *)sqlite3_column_text(stmt,i);
          printf("Text: %s\n", text);
          columns[i].value=malloc(sizeof(char)*(strlen(text)+1));
          sprintf(columns[i].value,"%s",text);
          break;
        case DBTYPE_DOUBLE: 
          columns[i].value=malloc(sizeof(double));
          *(double *)columns[i].value = sqlite3_column_double(stmt,i);
          break;
        default:
          print_fatal("Unhandled database type: %d\n", columns[i].type);
      }
    }
  }
 

  sqlite3_finalize(stmt);


  return num_rows;

}

int db_row_write(char *table, database_column_t *columns, int num_columns){
  int i,ret;
  char sql[1024];
  char keys[1024];
  char markers[1024];
  sqlite3_stmt* stmt;

  memset(keys,0,sizeof(keys));
  memset(markers,0,sizeof(keys));

  for(i=0;i<num_columns;i++) {
    strcat(keys,",");
    strcat(keys,columns[i].name);

    strcat(markers,",?");
  }

  sprintf(sql,"INSERT INTO %s(id%s) VALUES(NULL%s);", table,keys,markers);

  printf("SQL(%ld): '%s'\n",strlen(sql),sql);

  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return -1;
  }

  for(i=0;i<num_columns;i++) {
    if (columns[i].value == NULL) {
      sqlite3_bind_null(stmt,i+1);
      continue;
    }
      
    switch(columns[i].type){
      case DBTYPE_INT:
        if( (ret=sqlite3_bind_int(stmt,i+1,*(int *)columns[i].value)) != SQLITE_OK) {
          print_error("Error binding integer: %s\n", sqlite3_errmsg(db));
        }
        break;
      case DBTYPE_STRING:
        if ( (ret=sqlite3_bind_text(stmt,i+1,columns[i].value,strlen(columns[i].value),NULL)) != SQLITE_OK) {
          print_error("Error binding integer: %s\n", sqlite3_errmsg(db));
        }
        break;
      case DBTYPE_DOUBLE: 
        if ( (ret=sqlite3_bind_double(stmt, i+1, *(double *)columns[i].value)) != SQLITE_OK) {
          print_error("Error binding double: %s\n", sqlite3_errmsg(db));
        }
        break;
      default:
        print_fatal("Unhandled database type: %d\n", columns[i].type);
    }
  }

  while ((ret=sqlite3_step(stmt)) == SQLITE_ROW){
    printf("stmt executed\n");
  }
  if(ret != SQLITE_DONE) {
    print_error("Error executing '%s' -> %s\n", sql, sqlite3_errmsg(db));
    return -1;
  }

  sqlite3_finalize(stmt);


  return 0;

}

int64_t db_int64_get(char *table, char *key) {
  int id;
  int64_t value=0;
  sqlite3_stmt* stmt;
  char *str;
  char sql[512];
  
  sprintf(sql,"SELECT id,value FROM %s WHERE key = ?",table);
  
  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW)
  {
    id = sqlite3_column_int(stmt, 0);
    value=sqlite3_column_int64(stmt,1);
  }


  sqlite3_finalize(stmt);
  return value;
}

int db_sample_insert(struct json_object *sample) {
  int status=0;
  int err;
  struct json_object *code, *timestamp, *value;

  if(!json_object_object_get_ex(sample,"code",&code)){
    print_error("code missing from sample object\n");
    return -1;
  }

  if(!json_object_object_get_ex(sample,"timestamp",&timestamp)){
    print_error("timestamp missing from sample object\n");
    return -1;
  }

  if(!json_object_object_get_ex(sample,"value",&value)){
    print_error("value missing from sample object\n");
    return -1;
  }

  pthread_mutex_lock(&db_mutex);
  sqlite3_reset(db_sample_insert_stmt);

  if(err=sqlite3_bind_text(db_sample_insert_stmt, 1, json_object_get_string(code),-1, NULL) != SQLITE_OK) {
    print_error("Error binding code to sample stmt: %d", err);
    status=-1;
    goto cleanup;
  }

  if(err=sqlite3_bind_text(db_sample_insert_stmt, 2, json_object_get_string(timestamp),-1, NULL) != SQLITE_OK) {
    print_error("Error binding timestamp to sample stmt: %d", err);
    status=-1;
    goto cleanup;
  }

  if(err=sqlite3_bind_double(db_sample_insert_stmt, 3, json_object_get_double(value)) != SQLITE_OK) {
    print_error("Error binding value to sample stmt: %d", err);
    status=-1;
    goto cleanup;
  }

  while ((err=sqlite3_step(db_sample_insert_stmt)) != SQLITE_DONE) {
    print_info("Running sample insert: %d\n", err);
  }

cleanup:
  pthread_mutex_unlock(&db_mutex);
  return status;
}

int db_sample_sent(struct json_object *sample, int remove) {
  int status=0;
  int err;
  struct json_object *id;
  sqlite3_stmt *stmt=db_sample_is_sent_stmt;

  if(remove) {
    stmt=db_sample_delete_stmt;
  }
  
  if(!json_object_object_get_ex(sample,"id", &id)){
    debug_printf("No id in sample, not updating database\n");
    return 0;
  }

  pthread_mutex_lock(&db_mutex);
  sqlite3_reset(stmt);

  if((err=sqlite3_bind_int(stmt, 1, json_object_get_int(id))) != SQLITE_OK) {
    print_error("Error binding id: %d\n", err);
    goto cleanup;
  }

  while(sqlite3_step(stmt) != SQLITE_DONE) {}; 

cleanup:
  pthread_mutex_unlock(&db_mutex);
  return status;
}

struct json_object *db_samples_read(int limit) {
  int err;
  int id;
  char *code, *timestamp;
  double value;
  sqlite3_stmt *stmt=db_samples_read_stmt;
  struct json_object *sample;
  struct json_object *samples = json_object_new_array();

  pthread_mutex_lock(&db_mutex);
  sqlite3_reset(stmt);  

  
  if(err=sqlite3_bind_int(stmt, 1,0) != SQLITE_OK) {
    print_error("Could not bind is_sent: %d\n", err);
    goto cleanup;
  }

  if(err=sqlite3_bind_int(stmt, 2,limit) != SQLITE_OK) {
    print_error("Could not bind limit: %d\n", err);
    goto cleanup;
  }

  debug_printf("Reading samples\n");
  while( (err=sqlite3_step(stmt)) != SQLITE_DONE){
    if(err == SQLITE_ROW) {
      id=sqlite3_column_int(stmt,0);
      code = (char *)sqlite3_column_text(stmt,1);
      timestamp = (char *)sqlite3_column_text(stmt,2);
      value = sqlite3_column_double(stmt,3);

      sample=json_object_new_object();

      json_object_object_add(sample,"id", json_object_new_int(id));
      json_object_object_add(sample,"code", json_object_new_string(code));
      json_object_object_add(sample,"timestamp", json_object_new_string(timestamp));
      json_object_object_add(sample,"value", json_object_new_double(value));

      json_object_array_add(samples,sample);

    }else{
      print_error("Read samples error: %d\n", err);
    }
  }
  
cleanup:
  pthread_mutex_unlock(&db_mutex);
  return samples;
}
