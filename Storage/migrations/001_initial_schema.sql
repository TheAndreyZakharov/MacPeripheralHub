CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_at_unix_ms INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS profiles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    enforce_audio_defaults INTEGER NOT NULL DEFAULT 1,
    created_at_unix_ms INTEGER NOT NULL,
    updated_at_unix_ms INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS profile_device_roles (
    profile_id TEXT NOT NULL,
    role TEXT NOT NULL,
    device_id TEXT NOT NULL,
    PRIMARY KEY (profile_id, role),
    FOREIGN KEY (profile_id) REFERENCES profiles(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS known_devices (
    id TEXT PRIMARY KEY,
    category TEXT NOT NULL,
    transport TEXT NOT NULL,
    is_connected INTEGER NOT NULL,
    display_name TEXT NOT NULL,
    vendor_name TEXT NOT NULL,
    model_name TEXT NOT NULL,
    serial_number TEXT NOT NULL,
    last_seen_at_unix_ms INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS active_state (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    active_mode TEXT NOT NULL,
    active_profile_id TEXT NOT NULL,
    enforce_audio_defaults INTEGER NOT NULL,
    default_input_device_id TEXT NOT NULL,
    default_output_device_id TEXT NOT NULL,
    system_output_device_id TEXT NOT NULL,
    preferred_camera_device_id TEXT NOT NULL,
    updated_at_unix_ms INTEGER NOT NULL
);
