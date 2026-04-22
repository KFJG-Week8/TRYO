#ifndef SQL_H
#define SQL_H

#include <stddef.h>

#include "db.h"

#define SQL_MAX_SELECT_COLUMNS 3

typedef enum {
    SQL_INSERT,
    SQL_SELECT
} SqlType;

typedef enum {
    SQL_WHERE_NONE,
    SQL_WHERE_ID,
    SQL_WHERE_NAME
} SqlWhereType;

typedef enum {
    SQL_COLUMN_ID,
    SQL_COLUMN_NAME,
    SQL_COLUMN_AGE
} SqlColumn;

typedef struct {
    SqlType type;
    char table[32];
    int insert_has_id;
    int insert_id;
    char insert_name[DB_NAME_MAX];
    int insert_age;
    int select_all;
    SqlColumn select_columns[SQL_MAX_SELECT_COLUMNS];
    size_t select_column_count;
    SqlWhereType where_type;
    int where_id;
    char where_name[DB_NAME_MAX];
} SqlStatement;

int sql_parse(const char *sql, SqlStatement *stmt, char *err, size_t err_size);

#endif
