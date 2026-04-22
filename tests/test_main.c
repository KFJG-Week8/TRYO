#include "bptree.h"
#include "db.h"
#include "http.h"
#include "sql.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void test_sql_parser(void)
{
    SqlStatement stmt;
    char err[256];

    assert(sql_parse("INSERT INTO users name age VALUES 'kim' 20;", &stmt, err, sizeof(err)));
    assert(stmt.type == SQL_INSERT);
    assert(strcmp(stmt.insert_name, "kim") == 0);
    assert(stmt.insert_age == 20);

    assert(sql_parse("SELECT * FROM users;", &stmt, err, sizeof(err)));
    assert(stmt.type == SQL_SELECT);
    assert(stmt.where_type == SQL_WHERE_NONE);

    assert(sql_parse("SELECT * FROM users WHERE id = 42;", &stmt, err, sizeof(err)));
    assert(stmt.where_type == SQL_WHERE_ID);
    assert(stmt.where_id == 42);

    assert(sql_parse("SELECT * FROM users WHERE name = 'lee';", &stmt, err, sizeof(err)));
    assert(stmt.where_type == SQL_WHERE_NAME);
    assert(strcmp(stmt.where_name, "lee") == 0);

    assert(!sql_parse("DELETE FROM users WHERE id = 1;", &stmt, err, sizeof(err)));
}

static void test_bptree(void)
{
    BPlusTree tree;
    size_t value = 0;

    bptree_init(&tree);

    for (int i = 1; i <= 5000; i++) {
        assert(bptree_insert(&tree, i, (size_t)i * 10));
    }

    assert(tree.size == 5000);
    assert(bptree_search(&tree, 1, &value));
    assert(value == 10);
    assert(bptree_search(&tree, 2500, &value));
    assert(value == 25000);
    assert(bptree_search(&tree, 5000, &value));
    assert(value == 50000);
    assert(!bptree_search(&tree, 999999, &value));

    assert(bptree_insert(&tree, 2500, 77));
    assert(tree.size == 5000);
    assert(bptree_search(&tree, 2500, &value));
    assert(value == 77);

    bptree_destroy(&tree);
}

static void test_db(void)
{
    const char *path = "data/test_users.csv";
    DbEngine db;
    DbResult result;
    DbFilter filter;
    char err[256];

    remove(path);

    assert(db_init(&db, path, err, sizeof(err)));

    result = db_insert(&db, "kim", 20);
    assert(result.ok);
    assert(strstr(result.rows_json, "\"id\":1") != NULL);
    db_result_free(&result);

    result = db_insert(&db, "lee", 22);
    assert(result.ok);
    db_result_free(&result);

    memset(&filter, 0, sizeof(filter));
    filter.type = DB_FILTER_ID;
    filter.id = 1;
    result = db_select(&db, filter);
    assert(result.ok);
    assert(result.index_used);
    assert(strstr(result.rows_json, "\"name\":\"kim\"") != NULL);
    db_result_free(&result);

    memset(&filter, 0, sizeof(filter));
    filter.type = DB_FILTER_NAME;
    snprintf(filter.name, sizeof(filter.name), "lee");
    result = db_select(&db, filter);
    assert(result.ok);
    assert(!result.index_used);
    assert(strstr(result.rows_json, "\"age\":22") != NULL);
    db_result_free(&result);

    db_destroy(&db);

    assert(db_init(&db, path, err, sizeof(err)));
    memset(&filter, 0, sizeof(filter));
    filter.type = DB_FILTER_ID;
    filter.id = 2;
    result = db_select(&db, filter);
    assert(result.ok);
    assert(strstr(result.rows_json, "\"name\":\"lee\"") != NULL);
    db_result_free(&result);
    db_destroy(&db);

    remove(path);
}

static void test_http_sql_extract(void)
{
    char sql[256];
    char err[256];

    assert(http_extract_sql("{\"sql\":\"SELECT * FROM users WHERE id = 1;\"}", sql, sizeof(sql), err, sizeof(err)));
    assert(strcmp(sql, "SELECT * FROM users WHERE id = 1;") == 0);

    assert(http_extract_sql("{\"sql\":\"INSERT INTO users name age VALUES 'kim' 20;\"}", sql, sizeof(sql), err, sizeof(err)));
    assert(strcmp(sql, "INSERT INTO users name age VALUES 'kim' 20;") == 0);

    assert(!http_extract_sql("{\"query\":\"SELECT * FROM users;\"}", sql, sizeof(sql), err, sizeof(err)));
}

int main(void)
{
    test_sql_parser();
    test_bptree();
    test_db();
    test_http_sql_extract();

    printf("All tests passed\n");
    return 0;
}
