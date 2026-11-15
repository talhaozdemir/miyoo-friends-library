CREATE TABLE pairings (
  id TEXT PRIMARY KEY,
  device_mac TEXT NOT NULL,
  display_code TEXT NOT NULL,
  expires INTEGER NOT NULL,
  status TEXT NOT NULL DEFAULT 'pending',
  browser_mac TEXT,
  state_hash TEXT,
  oauth_cipher TEXT,
  token_cipher TEXT,
  error_code TEXT,
  last_poll INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX pairings_expiry ON pairings(expires);
-- No permanent Google tokens: only a non-reversible refresh-token digest.
CREATE TABLE grants (
  digest TEXT PRIMARY KEY,
  pairing_id TEXT NOT NULL,
  revoked INTEGER NOT NULL DEFAULT 0,
  last_refresh INTEGER NOT NULL DEFAULT 0,
  created INTEGER NOT NULL
);
CREATE INDEX grants_created ON grants(created);
CREATE TABLE rate_limits (bucket TEXT PRIMARY KEY, count INTEGER NOT NULL, expires INTEGER NOT NULL);
CREATE INDEX rate_expiry ON rate_limits(expires);
