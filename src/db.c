#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <math.h>
#include <phoenix.h>


static sqlite3 *db;
static char workpath[PATH_MAX-1];

int db_init(char *path) {
  int ret; 
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


int db_double_set(char *table, char *key, double value) {
  int ret;
  char sql[512];
  sqlite3_stmt* stmt;

  sprintf(sql,"INSERT INTO %s VALUES(NULL,?,?);", table);

  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, key, strlen(key),NULL);
  sqlite3_bind_double(stmt, 2, value);

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

    sqlite3_bind_double(stmt, 1, value);
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

int db_row_ids(char *table, int **ids) {
  sqlite3_stmt* stmt;
  char sql[512];
  int num_ids;

  print_info("Getting ids from %s\n", table);
  
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

  printf("SQL: %s\n",sql);

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
    printf("Binding column: %d\n",i);
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

