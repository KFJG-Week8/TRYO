#include "sql.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void set_err(char *err, size_t err_size, const char *message)
{
    if (err_size > 0) {
        snprintf(err, err_size, "%s", message);
    }
}

static const char *skip_ws(const char *p)
{
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static int read_word(const char **p, char *out, size_t out_size)
{
    const char *cur = skip_ws(*p);
    size_t len = 0;

    if (!(isalpha((unsigned char)*cur) || *cur == '_')) {
        return 0;
    }

    while (isalnum((unsigned char)*cur) || *cur == '_') {
        if (len + 1 >= out_size) {
            return 0;
        }
        out[len++] = *cur;
        cur++;
    }

    out[len] = '\0';
    *p = cur;
    return 1;
}

static int expect_keyword(const char **p, const char *keyword)
{
    char word[64];

    if (!read_word(p, word, sizeof(word))) {
        return 0;
    }

    return strcasecmp(word, keyword) == 0;
}

static int expect_symbol(const char **p, char symbol)
{
    const char *cur = skip_ws(*p);

    if (*cur != symbol) {
        return 0;
    }

    *p = cur + 1;
    return 1;
}

static int parse_int_value(const char **p, int *out)
{
    char *end = NULL;
    long value;
    const char *cur = skip_ws(*p);

    if (*cur == '\0') {
        return 0;
    }

    value = strtol(cur, &end, 10);
    if (end == cur || value < 0 || value > 2147483647L) {
        return 0;
    }

    *out = (int)value;
    *p = end;
    return 1;
}

static int parse_string_literal(const char **p, char *out, size_t out_size)
{
    const char *cur = skip_ws(*p);
    size_t len = 0;

    if (*cur != '\'') {
        return 0;
    }
    cur++;

    while (*cur != '\0' && *cur != '\'') {
        if (len + 1 >= out_size) {
            return 0;
        }
        out[len++] = *cur;
        cur++;
    }

    if (*cur != '\'') {
        return 0;
    }

    out[len] = '\0';
    *p = cur + 1;
    return 1;
}

static int at_statement_end(const char *p)
{
    p = skip_ws(p);
    if (*p == ';') {
        p++;
        p = skip_ws(p);
    }
    return *p == '\0';
}

static int parse_insert(const char *sql, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *p = sql;
    char col1[32];
    char col2[32];

    if (!expect_keyword(&p, "INSERT") || !expect_keyword(&p, "INTO")) {
        set_err(err, err_size, "expected INSERT INTO");
        return 0;
    }

    if (!read_word(&p, stmt->table, sizeof(stmt->table)) || strcasecmp(stmt->table, "users") != 0) {
        set_err(err, err_size, "only table users is supported");
        return 0;
    }

    if (!read_word(&p, col1, sizeof(col1)) || !read_word(&p, col2, sizeof(col2)) ||
        strcasecmp(col1, "name") != 0 || strcasecmp(col2, "age") != 0) {
        set_err(err, err_size, "expected columns: name age");
        return 0;
    }

    if (!expect_keyword(&p, "VALUES")) {
        set_err(err, err_size, "expected VALUES");
        return 0;
    }

    if (!parse_string_literal(&p, stmt->insert_name, sizeof(stmt->insert_name))) {
        set_err(err, err_size, "expected single-quoted name");
        return 0;
    }

    if (!parse_int_value(&p, &stmt->insert_age)) {
        set_err(err, err_size, "expected non-negative age");
        return 0;
    }

    if (!at_statement_end(p)) {
        set_err(err, err_size, "unexpected trailing tokens");
        return 0;
    }

    stmt->type = SQL_INSERT;
    snprintf(stmt->table, sizeof(stmt->table), "users");
    return 1;
}

static int parse_select(const char *sql, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *p = sql;
    char column[32];

    if (!expect_keyword(&p, "SELECT") || !expect_symbol(&p, '*') || !expect_keyword(&p, "FROM")) {
        set_err(err, err_size, "expected SELECT * FROM");
        return 0;
    }

    if (!read_word(&p, stmt->table, sizeof(stmt->table)) || strcasecmp(stmt->table, "users") != 0) {
        set_err(err, err_size, "only table users is supported");
        return 0;
    }

    stmt->type = SQL_SELECT;
    stmt->where_type = SQL_WHERE_NONE;
    snprintf(stmt->table, sizeof(stmt->table), "users");

    if (at_statement_end(p)) {
        return 1;
    }

    if (!expect_keyword(&p, "WHERE")) {
        set_err(err, err_size, "expected WHERE or statement end");
        return 0;
    }

    if (!read_word(&p, column, sizeof(column)) || !expect_symbol(&p, '=')) {
        set_err(err, err_size, "expected WHERE column = value");
        return 0;
    }

    if (strcasecmp(column, "id") == 0) {
        stmt->where_type = SQL_WHERE_ID;
        if (!parse_int_value(&p, &stmt->where_id)) {
            set_err(err, err_size, "expected numeric id");
            return 0;
        }
    } else if (strcasecmp(column, "name") == 0) {
        stmt->where_type = SQL_WHERE_NAME;
        if (!parse_string_literal(&p, stmt->where_name, sizeof(stmt->where_name))) {
            set_err(err, err_size, "expected single-quoted name");
            return 0;
        }
    } else {
        set_err(err, err_size, "only WHERE id or WHERE name is supported");
        return 0;
    }

    if (!at_statement_end(p)) {
        set_err(err, err_size, "unexpected trailing tokens");
        return 0;
    }

    return 1;
}

int sql_parse(const char *sql, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *p = skip_ws(sql);

    memset(stmt, 0, sizeof(*stmt));

    if (*p == '\0') {
        set_err(err, err_size, "empty SQL");
        return 0;
    }

    if (strncasecmp(p, "INSERT", 6) == 0) {
        return parse_insert(p, stmt, err, err_size);
    }

    if (strncasecmp(p, "SELECT", 6) == 0) {
        return parse_select(p, stmt, err, err_size);
    }

    set_err(err, err_size, "only INSERT and SELECT are supported");
    return 0;
}
