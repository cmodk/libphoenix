#include <stdio.h>
#include <string.h>
#include <linux/limits.h>
#include <math.h>
#include <phoenix.h>


static sqlite3 *db;
static char workpath[PATH_MAX-1];

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
  int i;
  for(i=0; i<argc; i++){
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

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
  double value;
  sqlite3_stmt* stmt;
  char *str;
  char sql[512];
  
  sprintf(sql,"SELECT id,value FROM %s WHERE key = ?",table);
  
  if(sqlite3_prepare(db,sql,strlen(sql), &stmt, NULL)!=SQLITE_OK) {
    print_error("Error preparing statement: %s\n", sqlite3_errmsg(db));
    return nan(NULL);
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

