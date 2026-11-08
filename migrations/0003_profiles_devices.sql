-- Profile fields already exist on users; add a one-way device credential for ESP32 authentication.
ALTER TABLE device_links ADD COLUMN token_hash TEXT;
CREATE UNIQUE INDEX IF NOT EXISTS device_links_token_hash ON device_links(token_hash) WHERE token_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS device_links_pair_expiry ON device_links(code_expires_at);
