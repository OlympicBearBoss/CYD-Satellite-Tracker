ALTER TABLE device_links ADD COLUMN telemetry_json TEXT;
ALTER TABLE device_links ADD COLUMN telemetry_at INTEGER;
CREATE TABLE IF NOT EXISTS device_commands (
  id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL REFERENCES device_links(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  command TEXT NOT NULL,
  payload_json TEXT NOT NULL DEFAULT '{}',
  created_at INTEGER NOT NULL,
  delivered_at INTEGER,
  completed_at INTEGER,
  result TEXT
);
CREATE INDEX IF NOT EXISTS device_commands_pending ON device_commands(device_id, delivered_at, created_at);
