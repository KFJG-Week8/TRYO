#ifndef DB_H
#define DB_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include "bptree.h"

#define DB_NAME_MAX 128
#define DB_PATH_MAX 512
#define DB_MAX_PROJECTION_COLUMNS 3

typedef struct {
    int id;
    char name[DB_NAME_MAX];
    int age;
} Record;

typedef enum {
    DB_FILTER_ALL,
    DB_FILTER_ID,
    DB_FILTER_NAME
} DbFilterType;

typedef struct {
    DbFilterType type;
    int id;
    char name[DB_NAME_MAX];
} DbFilter;

typedef enum {
    DB_COLUMN_ID,
    DB_COLUMN_NAME,
    DB_COLUMN_AGE
} DbColumn;

typedef struct {
    bool include_id;
    bool include_name;
    bool include_age;
    DbColumn columns[DB_MAX_PROJECTION_COLUMNS];
    size_t column_count;
} DbProjection;

typedef struct {
    bool ok;
    char *rows_json;
    char message[128];
    bool index_used;
    long long elapsed_us;
} DbResult;

typedef struct {
    char data_path[DB_PATH_MAX];
    Record *records;
    size_t count;
    size_t capacity;
    int next_id;
    BPlusTree index;
    pthread_rwlock_t lock;
} DbEngine;

int db_init(DbEngine *db, const char *data_path, char *err, size_t err_size);
void db_destroy(DbEngine *db);
DbResult db_insert(DbEngine *db, const char *name, int age);
DbResult db_insert_with_id(DbEngine *db, int id, const char *name, int age);
DbResult db_select(DbEngine *db, DbFilter filter);
DbResult db_select_projected(DbEngine *db, DbFilter filter, DbProjection projection);
void db_result_free(DbResult *result);

#endif
