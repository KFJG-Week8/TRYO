#ifndef SQL_H
#define SQL_H

#include <stddef.h>

#include "db.h"

typedef enum {
    SQL_INSERT,
    SQL_SELECT
} SqlType;

typedef enum {
    SQL_WHERE_NONE,
    SQL_WHERE_ID,
    SQL_WHERE_NAME
} SqlWhereType;

typedef struct {
    SqlType type;
    char table[32];
    char insert_name[DB_NAME_MAX];
    int insert_age;
    SqlWhereType where_type;
    int where_id;
    char where_name[DB_NAME_MAX];
} SqlStatement;

int sql_parse(const char *sql, SqlStatement *stmt, char *err, size_t err_size);

#endif
