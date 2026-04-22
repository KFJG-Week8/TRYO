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

static int parse_column_name(const char **p, SqlColumn *column)
{
    char word[32];

    if (!read_word(p, word, sizeof(word))) {
        return 0;
    }

    if (strcasecmp(word, "id") == 0) {
        *column = SQL_COLUMN_ID;
        return 1;
    }

    if (strcasecmp(word, "name") == 0) {
        *column = SQL_COLUMN_NAME;
        return 1;
    }

    if (strcasecmp(word, "age") == 0) {
        *column = SQL_COLUMN_AGE;
        return 1;
    }

    return 0;
}

static int column_already_selected(const SqlStatement *stmt, SqlColumn column)
{
    for (size_t i = 0; i < stmt->select_column_count; i++) {
        if (stmt->select_columns[i] == column) {
            return 1;
        }
    }

    return 0;
}

static int parse_select_columns(const char **p, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *cur = skip_ws(*p);

    stmt->select_all = 0;
    stmt->select_column_count = 0;

    if (*cur == '*') {
        stmt->select_all = 1;
        *p = cur + 1;
        return 1;
    }

    while (1) {
        SqlColumn column;

        if (stmt->select_column_count >= SQL_MAX_SELECT_COLUMNS) {
            set_err(err, err_size, "too many SELECT columns");
            return 0;
        }

        if (!parse_column_name(p, &column)) {
            set_err(err, err_size, "expected SELECT column: id, name, or age");
            return 0;
        }

        if (column_already_selected(stmt, column)) {
            set_err(err, err_size, "duplicate SELECT column");
            return 0;
        }

        stmt->select_columns[stmt->select_column_count++] = column;

        cur = skip_ws(*p);
        if (*cur != ',') {
            break;
        }
        *p = cur + 1;
    }

    return stmt->select_column_count > 0;
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

static int parse_insert_values_with_id(const char **p, SqlStatement *stmt, char *err, size_t err_size)
{
    int id;

    if (!expect_symbol(p, '(')) {
        set_err(err, err_size, "expected '(' after VALUES");
        return 0;
    }

    if (!parse_int_value(p, &id) || id <= 0) {
        set_err(err, err_size, "expected positive id");
        return 0;
    }

    if (!expect_symbol(p, ',')) {
        set_err(err, err_size, "expected comma after id");
        return 0;
    }

    if (!parse_string_literal(p, stmt->insert_name, sizeof(stmt->insert_name))) {
        set_err(err, err_size, "expected single-quoted name");
        return 0;
    }

    if (!expect_symbol(p, ',')) {
        set_err(err, err_size, "expected comma after name");
        return 0;
    }

    if (!parse_int_value(p, &stmt->insert_age)) {
        set_err(err, err_size, "expected non-negative age");
        return 0;
    }

    if (!expect_symbol(p, ')')) {
        set_err(err, err_size, "expected ')' after VALUES");
        return 0;
    }

    if (!at_statement_end(*p)) {
        set_err(err, err_size, "unexpected trailing tokens");
        return 0;
    }

    stmt->insert_has_id = 1;
    stmt->insert_id = id;
    return 1;
}

static int parse_insert(const char *sql, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *p = sql;
    const char *after_table;
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

    after_table = p;

    if (expect_keyword(&p, "VALUES")) {
        if (!parse_insert_values_with_id(&p, stmt, err, err_size)) {
            return 0;
        }
        stmt->type = SQL_INSERT;
        snprintf(stmt->table, sizeof(stmt->table), "users");
        return 1;
    }

    p = after_table;
    if (!read_word(&p, col1, sizeof(col1)) || !read_word(&p, col2, sizeof(col2)) ||
        strcasecmp(col1, "name") != 0 || strcasecmp(col2, "age") != 0) {
        set_err(err, err_size, "expected VALUES (id, 'name', age) or columns: name age");
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
    stmt->insert_has_id = 0;
    snprintf(stmt->table, sizeof(stmt->table), "users");
    return 1;
}

static int parse_select(const char *sql, SqlStatement *stmt, char *err, size_t err_size)
{
    const char *p = sql;
    char column[32];

    if (!expect_keyword(&p, "SELECT")) {
        set_err(err, err_size, "expected SELECT");
        return 0;
    }

    if (!parse_select_columns(&p, stmt, err, err_size)) {
        return 0;
    }

    if (!expect_keyword(&p, "FROM")) {
        set_err(err, err_size, "expected FROM");
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
