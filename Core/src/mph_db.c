#include "mph_db.h"

#include "mph_log.h"

#include <errno.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

struct mph_db {
    sqlite3 *connection;
    char path[MPH_DB_PATH_CAPACITY];
};

typedef struct {
    int version;
    const char *name;
    const char *sql;
} mph_db_migration_t;

static const char migration_001_sql[] =
    "CREATE TABLE IF NOT EXISTS schema_migrations ("
    "version INTEGER PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "applied_at_unix_ms INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS profiles ("
    "id TEXT PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "enforce_audio_defaults INTEGER NOT NULL DEFAULT 1,"
    "created_at_unix_ms INTEGER NOT NULL,"
    "updated_at_unix_ms INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS profile_device_roles ("
    "profile_id TEXT NOT NULL,"
    "role TEXT NOT NULL,"
    "device_id TEXT NOT NULL,"
    "PRIMARY KEY (profile_id, role),"
    "FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE"
    ");"
    "CREATE TABLE IF NOT EXISTS known_devices ("
    "id TEXT PRIMARY KEY,"
    "category TEXT NOT NULL,"
    "transport TEXT NOT NULL,"
    "is_connected INTEGER NOT NULL,"
    "display_name TEXT NOT NULL,"
    "vendor_name TEXT NOT NULL,"
    "model_name TEXT NOT NULL,"
    "serial_number TEXT NOT NULL,"
    "last_seen_at_unix_ms INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS active_state ("
    "id INTEGER PRIMARY KEY CHECK (id = 1),"
    "active_mode TEXT NOT NULL,"
    "active_profile_id TEXT NOT NULL,"
    "enforce_audio_defaults INTEGER NOT NULL,"
    "default_input_device_id TEXT NOT NULL,"
    "default_output_device_id TEXT NOT NULL,"
    "system_output_device_id TEXT NOT NULL,"
    "preferred_camera_device_id TEXT NOT NULL,"
    "updated_at_unix_ms INTEGER NOT NULL"
    ");";

static const mph_db_migration_t migrations[] = {
    {1, "initial_schema", migration_001_sql},
};

static mph_status_t execute_sql(mph_db_t *db, const char *sql) {
    if (db == NULL || db->connection == NULL || sql == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    char *sqlite_error = NULL;
    int rc = sqlite3_exec(db->connection, sql, NULL, NULL, &sqlite_error);
    if (rc != SQLITE_OK) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db",
                        sqlite_error != NULL ? sqlite_error : "sqlite exec failed");
        sqlite3_free(sqlite_error);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    return MPH_STATUS_OK;
}

static mph_status_t prepare_statement(mph_db_t *db, const char *sql, sqlite3_stmt **out_statement) {
    if (db == NULL || db->connection == NULL || sql == NULL || out_statement == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    int rc = sqlite3_prepare_v2(db->connection, sql, -1, out_statement, NULL);
    if (rc != SQLITE_OK) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db", sqlite3_errmsg(db->connection));
        return MPH_STATUS_INTERNAL_ERROR;
    }

    return MPH_STATUS_OK;
}

static mph_status_t step_done(sqlite3 *connection, sqlite3_stmt *statement) {
    int rc = sqlite3_step(statement);
    if (rc != SQLITE_DONE) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db", sqlite3_errmsg(connection));
        return MPH_STATUS_INTERNAL_ERROR;
    }

    return MPH_STATUS_OK;
}

static const char *safe_text(const char *value) {
    return value != NULL ? value : "";
}

static mph_status_t bind_text(sqlite3_stmt *statement, int index, const char *value) {
    int rc = sqlite3_bind_text(statement, index, safe_text(value), -1, SQLITE_TRANSIENT);
    return rc == SQLITE_OK ? MPH_STATUS_OK : MPH_STATUS_INTERNAL_ERROR;
}

static const char *column_text(sqlite3_stmt *statement, int index) {
    const unsigned char *value = sqlite3_column_text(statement, index);
    return value != NULL ? (const char *)value : "";
}

static bool migration_applied(mph_db_t *db, int version) {
    sqlite3_stmt *statement = NULL;
    if (!mph_status_is_ok(prepare_statement(
            db, "SELECT 1 FROM schema_migrations WHERE version = ? LIMIT 1;", &statement))) {
        return false;
    }

    sqlite3_bind_int(statement, 1, version);
    bool found = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return found;
}

static mph_status_t record_migration(mph_db_t *db, const mph_db_migration_t *migration) {
    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(
        db,
        "INSERT OR REPLACE INTO schema_migrations (version, name, applied_at_unix_ms) "
        "VALUES (?, ?, ?);",
        &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    sqlite3_bind_int(statement, 1, migration->version);
    bind_text(statement, 2, migration->name);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)mph_time_now_unix_ms());
    status = step_done(db->connection, statement);
    sqlite3_finalize(statement);
    return status;
}

static mph_status_t ensure_directory(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return MPH_STATUS_OK;
    }

    return MPH_STATUS_INTERNAL_ERROR;
}

static const char *active_mode_name(mph_active_mode_t mode) {
    switch (mode) {
    case MPH_ACTIVE_MODE_NONE:
        return "none";
    case MPH_ACTIVE_MODE_PROFILE:
        return "profile";
    case MPH_ACTIVE_MODE_MANUAL:
        return "manual";
    }

    return "none";
}

static mph_active_mode_t active_mode_from_name(const char *name) {
    if (strcmp(name, "profile") == 0) {
        return MPH_ACTIVE_MODE_PROFILE;
    }
    if (strcmp(name, "manual") == 0) {
        return MPH_ACTIVE_MODE_MANUAL;
    }
    return MPH_ACTIVE_MODE_NONE;
}

static bool role_from_name(const char *name, mph_device_role_t *out_role) {
    if (name == NULL || out_role == NULL) {
        return false;
    }

    for (int role = 0; role < MPH_DEVICE_ROLE_COUNT; role += 1) {
        if (strcmp(name, mph_device_role_name((mph_device_role_t)role)) == 0) {
            *out_role = (mph_device_role_t)role;
            return true;
        }
    }

    return false;
}

static mph_status_t set_selection_role_from_text(mph_selection_t *selection, mph_device_role_t role,
                                                 const char *device_id_text) {
    mph_device_id_t device_id;
    mph_device_id_clear(&device_id);

    if (device_id_text != NULL && device_id_text[0] != '\0') {
        mph_status_t status = mph_device_id_set(&device_id, device_id_text);
        if (!mph_status_is_ok(status)) {
            return status;
        }
    }

    return mph_selection_set_role_device(selection, role, &device_id);
}

mph_status_t mph_db_default_path(char *buffer, size_t capacity) {
    if (buffer == NULL || capacity == 0) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return MPH_STATUS_NOT_FOUND;
    }

    int written =
        snprintf(buffer, capacity,
                 "%s/Library/Application Support/MacPeripheralHub/MacPeripheralHub.sqlite3", home);
    if (written < 0 || (size_t)written >= capacity) {
        buffer[0] = '\0';
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    return MPH_STATUS_OK;
}

mph_status_t mph_db_open(mph_db_t **out_db, const char *path) {
    if (out_db == NULL || path == NULL || path[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    *out_db = NULL;
    mph_db_t *db = calloc(1, sizeof(*db));
    if (db == NULL) {
        return MPH_STATUS_NO_MEMORY;
    }

    int written = snprintf(db->path, sizeof(db->path), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(db->path)) {
        free(db);
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    int rc =
        sqlite3_open_v2(path, &db->connection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db",
                        db->connection != NULL ? sqlite3_errmsg(db->connection)
                                               : "sqlite open failed");
        mph_db_close(db);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    execute_sql(db, "PRAGMA foreign_keys = ON;");
    *out_db = db;
    return MPH_STATUS_OK;
}

mph_status_t mph_db_open_application_support(mph_db_t **out_db) {
    char path[MPH_DB_PATH_CAPACITY];
    mph_status_t status = mph_db_default_path(path, sizeof(path));
    if (!mph_status_is_ok(status)) {
        return status;
    }

    const char *home = getenv("HOME");
    char library_path[MPH_DB_PATH_CAPACITY];
    char app_support_path[MPH_DB_PATH_CAPACITY];
    char app_path[MPH_DB_PATH_CAPACITY];

    int written = snprintf(library_path, sizeof(library_path), "%s/Library", home);
    if (written < 0 || (size_t)written >= sizeof(library_path)) {
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }
    written = snprintf(app_support_path, sizeof(app_support_path), "%s/Application Support",
                       library_path);
    if (written < 0 || (size_t)written >= sizeof(app_support_path)) {
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }
    written = snprintf(app_path, sizeof(app_path), "%s/MacPeripheralHub", app_support_path);
    if (written < 0 || (size_t)written >= sizeof(app_path)) {
        return MPH_STATUS_CAPACITY_EXCEEDED;
    }

    status = ensure_directory(library_path);
    if (!mph_status_is_ok(status)) {
        return status;
    }
    status = ensure_directory(app_support_path);
    if (!mph_status_is_ok(status)) {
        return status;
    }
    status = ensure_directory(app_path);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    return mph_db_open(out_db, path);
}

void mph_db_close(mph_db_t *db) {
    if (db == NULL) {
        return;
    }

    if (db->connection != NULL) {
        sqlite3_close(db->connection);
    }
    free(db);
}

const char *mph_db_path(const mph_db_t *db) {
    return db != NULL ? db->path : "";
}

mph_status_t mph_db_migrate(mph_db_t *db) {
    if (db == NULL || db->connection == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = execute_sql(db, "BEGIN IMMEDIATE;");
    if (!mph_status_is_ok(status)) {
        return status;
    }

    status = execute_sql(db, "CREATE TABLE IF NOT EXISTS schema_migrations ("
                             "version INTEGER PRIMARY KEY,"
                             "name TEXT NOT NULL,"
                             "applied_at_unix_ms INTEGER NOT NULL"
                             ");");
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }

    for (size_t index = 0; index < sizeof(migrations) / sizeof(migrations[0]); index += 1) {
        if (migration_applied(db, migrations[index].version)) {
            continue;
        }

        status = execute_sql(db, migrations[index].sql);
        if (!mph_status_is_ok(status)) {
            execute_sql(db, "ROLLBACK;");
            return status;
        }

        status = record_migration(db, &migrations[index]);
        if (!mph_status_is_ok(status)) {
            execute_sql(db, "ROLLBACK;");
            return status;
        }
    }

    return execute_sql(db, "COMMIT;");
}

int mph_db_schema_version(const mph_db_t *db) {
    if (db == NULL || db->connection == NULL) {
        return 0;
    }

    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(db->connection, "SELECT MAX(version) FROM schema_migrations;", -1,
                           &statement, NULL) != SQLITE_OK) {
        return 0;
    }

    int version = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        version = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return version;
}

mph_status_t mph_db_save_profile(mph_db_t *db, const mph_profile_t *profile) {
    if (db == NULL || !mph_profile_is_valid(profile)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    mph_status_t status = execute_sql(db, "BEGIN IMMEDIATE;");
    if (!mph_status_is_ok(status)) {
        return status;
    }

    sqlite3_stmt *statement = NULL;
    status = prepare_statement(
        db,
        "INSERT INTO profiles "
        "(id, name, enforce_audio_defaults, created_at_unix_ms, updated_at_unix_ms) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name = excluded.name,"
        "enforce_audio_defaults = excluded.enforce_audio_defaults,"
        "updated_at_unix_ms = excluded.updated_at_unix_ms;",
        &statement);
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }

    bind_text(statement, 1, profile->id);
    bind_text(statement, 2, profile->name);
    sqlite3_bind_int(statement, 3, profile->selection.enforce_audio_defaults ? 1 : 0);
    sqlite3_bind_int64(statement, 4, (sqlite3_int64)profile->created_at_unix_ms);
    sqlite3_bind_int64(statement, 5, (sqlite3_int64)profile->updated_at_unix_ms);
    status = step_done(db->connection, statement);
    sqlite3_finalize(statement);
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }

    status =
        prepare_statement(db, "DELETE FROM profile_device_roles WHERE profile_id = ?;", &statement);
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }
    bind_text(statement, 1, profile->id);
    status = step_done(db->connection, statement);
    sqlite3_finalize(statement);
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }

    status = prepare_statement(
        db, "INSERT INTO profile_device_roles (profile_id, role, device_id) VALUES (?, ?, ?);",
        &statement);
    if (!mph_status_is_ok(status)) {
        execute_sql(db, "ROLLBACK;");
        return status;
    }

    for (int role = 0; role < MPH_DEVICE_ROLE_COUNT; role += 1) {
        const mph_device_id_t *device_id =
            mph_selection_get_role_device(&profile->selection, (mph_device_role_t)role);
        if (mph_device_id_is_empty(device_id)) {
            continue;
        }

        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        bind_text(statement, 1, profile->id);
        bind_text(statement, 2, mph_device_role_name((mph_device_role_t)role));
        bind_text(statement, 3, mph_device_id_cstr(device_id));
        status = step_done(db->connection, statement);
        if (!mph_status_is_ok(status)) {
            sqlite3_finalize(statement);
            execute_sql(db, "ROLLBACK;");
            return status;
        }
    }

    sqlite3_finalize(statement);
    return execute_sql(db, "COMMIT;");
}

mph_status_t mph_db_load_profile(mph_db_t *db, const char *profile_id, mph_profile_t *out_profile,
                                 bool *out_found) {
    if (db == NULL || profile_id == NULL || out_profile == NULL || out_found == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    *out_found = false;
    mph_profile_init(out_profile);

    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(
        db,
        "SELECT id, name, enforce_audio_defaults, created_at_unix_ms, updated_at_unix_ms "
        "FROM profiles WHERE id = ?;",
        &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    bind_text(statement, 1, profile_id);
    int rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return MPH_STATUS_OK;
    }
    if (rc != SQLITE_ROW) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db", sqlite3_errmsg(db->connection));
        sqlite3_finalize(statement);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    status =
        mph_profile_configure(out_profile, column_text(statement, 0), column_text(statement, 1));
    if (mph_status_is_ok(status)) {
        out_profile->selection.enforce_audio_defaults = sqlite3_column_int(statement, 2) != 0;
        out_profile->created_at_unix_ms = (uint64_t)sqlite3_column_int64(statement, 3);
        out_profile->updated_at_unix_ms = (uint64_t)sqlite3_column_int64(statement, 4);
        *out_found = true;
    }
    sqlite3_finalize(statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    status = prepare_statement(
        db, "SELECT role, device_id FROM profile_device_roles WHERE profile_id = ?;", &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    bind_text(statement, 1, profile_id);
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        mph_device_role_t role;
        if (!role_from_name(column_text(statement, 0), &role)) {
            continue;
        }

        status =
            set_selection_role_from_text(&out_profile->selection, role, column_text(statement, 1));
        if (!mph_status_is_ok(status)) {
            sqlite3_finalize(statement);
            return status;
        }
    }

    sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? MPH_STATUS_OK : MPH_STATUS_INTERNAL_ERROR;
}

mph_status_t mph_db_load_profiles(mph_db_t *db, mph_profile_store_t *store) {
    if (db == NULL || store == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = NULL;
    mph_status_t status =
        prepare_statement(db, "SELECT id FROM profiles ORDER BY name, id;", &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        mph_profile_t profile;
        bool found = false;
        status = mph_db_load_profile(db, column_text(statement, 0), &profile, &found);
        if (!mph_status_is_ok(status)) {
            sqlite3_finalize(statement);
            return status;
        }
        if (found) {
            status = mph_profile_store_save(store, &profile);
            if (!mph_status_is_ok(status)) {
                sqlite3_finalize(statement);
                return status;
            }
        }
    }

    sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? MPH_STATUS_OK : MPH_STATUS_INTERNAL_ERROR;
}

mph_status_t mph_db_delete_profile(mph_db_t *db, const char *profile_id) {
    if (db == NULL || profile_id == NULL || profile_id[0] == '\0') {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(db, "DELETE FROM profiles WHERE id = ?;", &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    bind_text(statement, 1, profile_id);
    status = step_done(db->connection, statement);
    int changed = sqlite3_changes(db->connection);
    sqlite3_finalize(statement);
    return changed > 0 ? status : MPH_STATUS_NOT_FOUND;
}

mph_status_t mph_db_save_known_device(mph_db_t *db, const mph_device_t *device) {
    if (db == NULL || device == NULL || mph_device_id_is_empty(&device->id)) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(
        db,
        "INSERT INTO known_devices "
        "(id, category, transport, is_connected, display_name, vendor_name, model_name, "
        "serial_number, last_seen_at_unix_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "category = excluded.category,"
        "transport = excluded.transport,"
        "is_connected = excluded.is_connected,"
        "display_name = excluded.display_name,"
        "vendor_name = excluded.vendor_name,"
        "model_name = excluded.model_name,"
        "serial_number = excluded.serial_number,"
        "last_seen_at_unix_ms = excluded.last_seen_at_unix_ms;",
        &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    bind_text(statement, 1, mph_device_id_cstr(&device->id));
    bind_text(statement, 2, mph_device_category_name(device->category));
    bind_text(statement, 3, mph_device_transport_name(device->transport));
    sqlite3_bind_int(statement, 4, device->is_connected ? 1 : 0);
    bind_text(statement, 5, device->display_name);
    bind_text(statement, 6, device->vendor_name);
    bind_text(statement, 7, device->model_name);
    bind_text(statement, 8, device->serial_number);
    sqlite3_bind_int64(statement, 9, (sqlite3_int64)mph_time_now_unix_ms());

    status = step_done(db->connection, statement);
    sqlite3_finalize(statement);
    return status;
}

mph_status_t mph_db_save_active_selection(mph_db_t *db, const mph_selection_t *selection) {
    if (db == NULL || selection == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(
        db,
        "INSERT INTO active_state "
        "(id, active_mode, active_profile_id, enforce_audio_defaults, default_input_device_id, "
        "default_output_device_id, system_output_device_id, preferred_camera_device_id, "
        "updated_at_unix_ms) "
        "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "active_mode = excluded.active_mode,"
        "active_profile_id = excluded.active_profile_id,"
        "enforce_audio_defaults = excluded.enforce_audio_defaults,"
        "default_input_device_id = excluded.default_input_device_id,"
        "default_output_device_id = excluded.default_output_device_id,"
        "system_output_device_id = excluded.system_output_device_id,"
        "preferred_camera_device_id = excluded.preferred_camera_device_id,"
        "updated_at_unix_ms = excluded.updated_at_unix_ms;",
        &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    bind_text(statement, 1, active_mode_name(selection->mode));
    bind_text(statement, 2, selection->profile_id);
    sqlite3_bind_int(statement, 3, selection->enforce_audio_defaults ? 1 : 0);
    bind_text(statement, 4,
              mph_device_id_cstr(&selection->role_device_ids[MPH_DEVICE_ROLE_DEFAULT_INPUT]));
    bind_text(statement, 5,
              mph_device_id_cstr(&selection->role_device_ids[MPH_DEVICE_ROLE_DEFAULT_OUTPUT]));
    bind_text(statement, 6,
              mph_device_id_cstr(&selection->role_device_ids[MPH_DEVICE_ROLE_SYSTEM_OUTPUT]));
    bind_text(statement, 7,
              mph_device_id_cstr(&selection->role_device_ids[MPH_DEVICE_ROLE_PREFERRED_CAMERA]));
    sqlite3_bind_int64(statement, 8, (sqlite3_int64)mph_time_now_unix_ms());

    status = step_done(db->connection, statement);
    sqlite3_finalize(statement);
    return status;
}

mph_status_t mph_db_load_active_selection(mph_db_t *db, mph_selection_t *out_selection,
                                          bool *out_found) {
    if (db == NULL || out_selection == NULL || out_found == NULL) {
        return MPH_STATUS_INVALID_ARGUMENT;
    }

    *out_found = false;
    mph_selection_init(out_selection);

    sqlite3_stmt *statement = NULL;
    mph_status_t status = prepare_statement(
        db,
        "SELECT active_mode, active_profile_id, enforce_audio_defaults, default_input_device_id, "
        "default_output_device_id, system_output_device_id, preferred_camera_device_id "
        "FROM active_state WHERE id = 1;",
        &statement);
    if (!mph_status_is_ok(status)) {
        return status;
    }

    int rc = sqlite3_step(statement);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(statement);
        return MPH_STATUS_OK;
    }
    if (rc != SQLITE_ROW) {
        mph_log_message(MPH_LOG_LEVEL_ERROR, "mph_db", sqlite3_errmsg(db->connection));
        sqlite3_finalize(statement);
        return MPH_STATUS_INTERNAL_ERROR;
    }

    out_selection->mode = active_mode_from_name(column_text(statement, 0));
    status = mph_selection_set_profile_id(out_selection, column_text(statement, 1));
    if (mph_status_is_ok(status)) {
        out_selection->enforce_audio_defaults = sqlite3_column_int(statement, 2) != 0;
    }
    if (mph_status_is_ok(status)) {
        status = set_selection_role_from_text(out_selection, MPH_DEVICE_ROLE_DEFAULT_INPUT,
                                              column_text(statement, 3));
    }
    if (mph_status_is_ok(status)) {
        status = set_selection_role_from_text(out_selection, MPH_DEVICE_ROLE_DEFAULT_OUTPUT,
                                              column_text(statement, 4));
    }
    if (mph_status_is_ok(status)) {
        status = set_selection_role_from_text(out_selection, MPH_DEVICE_ROLE_SYSTEM_OUTPUT,
                                              column_text(statement, 5));
    }
    if (mph_status_is_ok(status)) {
        status = set_selection_role_from_text(out_selection, MPH_DEVICE_ROLE_PREFERRED_CAMERA,
                                              column_text(statement, 6));
    }

    sqlite3_finalize(statement);
    *out_found = mph_status_is_ok(status);
    return status;
}
