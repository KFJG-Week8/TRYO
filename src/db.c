#include "db.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err, size_t err_size, const char *message)
{
    if (err_size > 0) {
        snprintf(err, err_size, "%s", message);
    }
}

static DbResult make_error_result(const char *message, long long start_us)
{
    DbResult result;

    result.ok = false;
    result.rows_json = NULL;
    snprintf(result.message, sizeof(result.message), "%s", message);
    result.index_used = false;
    result.elapsed_us = now_us() - start_us;
    return result;
}

static int db_reserve_records(DbEngine *db, size_t needed)
{
    size_t next_capacity;
    Record *next;

    if (needed <= db->capacity) {
        return 1;
    }

    next_capacity = db->capacity == 0 ? 64 : db->capacity;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }

    next = realloc(db->records, sizeof(Record) * next_capacity);
    if (next == NULL) {
        return 0;
    }

    db->records = next;
    db->capacity = next_capacity;
    return 1;
}

static int name_is_valid(const char *name)
{
    if (name[0] == '\0') {
        return 0;
    }

    for (size_t i = 0; name[i] != '\0'; i++) {
        if (name[i] == ',' || name[i] == '\n' || name[i] == '\r') {
            return 0;
        }
    }

    return 1;
}

static int append_record_json(JsonBuilder *builder, const Record *record)
{
    return json_builder_append(builder, "{\"id\":") &&
           json_builder_appendf(builder, "%d", record->id) &&
           json_builder_append(builder, ",\"name\":") &&
           json_builder_append_string(builder, record->name) &&
           json_builder_append(builder, ",\"age\":") &&
           json_builder_appendf(builder, "%d", record->age) &&
           json_builder_append(builder, "}");
}

static int load_record(DbEngine *db, const Record *record)
{
    if (!db_reserve_records(db, db->count + 1)) {
        return 0;
    }

    db->records[db->count] = *record;
    if (!bptree_insert(&db->index, record->id, db->count)) {
        return 0;
    }

    db->count++;
    if (record->id >= db->next_id) {
        db->next_id = record->id + 1;
    }
    return 1;
}

int db_init(DbEngine *db, const char *data_path, char *err, size_t err_size)
{
    FILE *file;
    char line[512];

    memset(db, 0, sizeof(*db));
    db->next_id = 1;

    if (strlen(data_path) >= sizeof(db->data_path)) {
        set_err(err, err_size, "data path is too long");
        return 0;
    }
    snprintf(db->data_path, sizeof(db->data_path), "%s", data_path);

    if (pthread_rwlock_init(&db->lock, NULL) != 0) {
        set_err(err, err_size, "failed to initialize DB lock");
        return 0;
    }

    bptree_init(&db->index);

    file = fopen(db->data_path, "a+");
    if (file == NULL) {
        set_err(err, err_size, "failed to open data file");
        pthread_rwlock_destroy(&db->lock);
        return 0;
    }

    rewind(file);
    while (fgets(line, sizeof(line), file) != NULL) {
        Record record;
        char name[DB_NAME_MAX];

        if (line[0] == '\n' || line[0] == '\0') {
            continue;
        }

        if (sscanf(line, "%d,%127[^,],%d", &record.id, name, &record.age) != 3) {
            fclose(file);
            db_destroy(db);
            set_err(err, err_size, "malformed data file");
            return 0;
        }

        snprintf(record.name, sizeof(record.name), "%s", name);
        if (record.id <= 0 || record.age < 0 || !name_is_valid(record.name)) {
            fclose(file);
            db_destroy(db);
            set_err(err, err_size, "invalid record in data file");
            return 0;
        }

        if (!load_record(db, &record)) {
            fclose(file);
            db_destroy(db);
            set_err(err, err_size, "failed to load data file into memory");
            return 0;
        }
    }

    fclose(file);
    return 1;
}

void db_destroy(DbEngine *db)
{
    free(db->records);
    db->records = NULL;
    db->count = 0;
    db->capacity = 0;
    bptree_destroy(&db->index);
    pthread_rwlock_destroy(&db->lock);
}

DbResult db_insert(DbEngine *db, const char *name, int age)
{
    long long start_us = now_us();
    DbResult result;
    Record record;
    JsonBuilder builder;
    FILE *file;
    size_t record_index;

    if (pthread_rwlock_wrlock(&db->lock) != 0) {
        return make_error_result("failed to acquire write lock", start_us);
    }

    if (age < 0 || !name_is_valid(name)) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("invalid name or age", start_us);
    }

    if (!db_reserve_records(db, db->count + 1)) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("out of memory", start_us);
    }

    record.id = db->next_id;
    snprintf(record.name, sizeof(record.name), "%s", name);
    record.age = age;
    record_index = db->count;

    file = fopen(db->data_path, "a");
    if (file == NULL) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to open data file for append", start_us);
    }

    int write_failed = fprintf(file, "%d,%s,%d\n", record.id, record.name, record.age) < 0;
    int close_failed = fclose(file) != 0;

    if (write_failed || close_failed) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to append data file", start_us);
    }

    db->records[record_index] = record;
    db->count++;
    db->next_id++;

    if (!bptree_insert(&db->index, record.id, record_index)) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to update B+ tree index", start_us);
    }

    if (!json_builder_init(&builder) || !json_builder_append(&builder, "[") ||
        !append_record_json(&builder, &record) || !json_builder_append(&builder, "]")) {
        json_builder_free(&builder);
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to serialize insert result", start_us);
    }

    result.ok = true;
    result.rows_json = json_builder_take(&builder);
    snprintf(result.message, sizeof(result.message), "inserted 1 row");
    result.index_used = false;
    result.elapsed_us = now_us() - start_us;

    pthread_rwlock_unlock(&db->lock);
    return result;
}

DbResult db_select(DbEngine *db, DbFilter filter)
{
    long long start_us = now_us();
    DbResult result;
    JsonBuilder builder;
    size_t matched = 0;
    int need_comma = 0;

    if (pthread_rwlock_rdlock(&db->lock) != 0) {
        return make_error_result("failed to acquire read lock", start_us);
    }

    if (!json_builder_init(&builder) || !json_builder_append(&builder, "[")) {
        json_builder_free(&builder);
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to serialize select result", start_us);
    }

    if (filter.type == DB_FILTER_ID) {
        size_t record_index = 0;

        if (bptree_search(&db->index, filter.id, &record_index) && record_index < db->count) {
            if (!append_record_json(&builder, &db->records[record_index])) {
                json_builder_free(&builder);
                pthread_rwlock_unlock(&db->lock);
                return make_error_result("failed to serialize indexed row", start_us);
            }
            matched++;
        }
    } else {
        for (size_t i = 0; i < db->count; i++) {
            int include = filter.type == DB_FILTER_ALL ||
                          (filter.type == DB_FILTER_NAME && strcmp(db->records[i].name, filter.name) == 0);

            if (!include) {
                continue;
            }

            if (need_comma && !json_builder_append(&builder, ",")) {
                json_builder_free(&builder);
                pthread_rwlock_unlock(&db->lock);
                return make_error_result("failed to serialize select separator", start_us);
            }

            if (!append_record_json(&builder, &db->records[i])) {
                json_builder_free(&builder);
                pthread_rwlock_unlock(&db->lock);
                return make_error_result("failed to serialize select row", start_us);
            }

            need_comma = 1;
            matched++;
        }
    }

    if (!json_builder_append(&builder, "]")) {
        json_builder_free(&builder);
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("failed to finalize select result", start_us);
    }

    result.ok = true;
    result.rows_json = json_builder_take(&builder);
    snprintf(result.message, sizeof(result.message), "selected %zu row(s)", matched);
    result.index_used = filter.type == DB_FILTER_ID;
    result.elapsed_us = now_us() - start_us;

    pthread_rwlock_unlock(&db->lock);
    return result;
}

void db_result_free(DbResult *result)
{
    free(result->rows_json);
    result->rows_json = NULL;
}
