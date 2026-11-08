-- Roles, account status and administrator audit trail.
ALTER TABLE users ADD COLUMN role TEXT NOT NULL DEFAULT 'user' CHECK(role IN ('user','admin'));
ALTER TABLE users ADD COLUMN status TEXT NOT NULL DEFAULT 'active' CHECK(status IN ('active','blocked'));
CREATE INDEX IF NOT EXISTS users_role_status ON users(role,status);
CREATE TABLE IF NOT EXISTS admin_audit (id TEXT PRIMARY KEY, actor_user_id TEXT NOT NULL, target_user_id TEXT, action TEXT NOT NULL, details TEXT NOT NULL DEFAULT '{}', created_at INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS admin_audit_created_at ON admin_audit(created_at);
