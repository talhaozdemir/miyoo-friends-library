# Optional HTTPS phone authentication service

Cloudflare Workers **Free** + D1 **Free**, for the small invited friends group.
No paid plan, Firebase, ROM proxy, upload endpoint or permanent plaintext token
database. The existing Desktop/rclone setup works independently of this service.

## Deploy your own instance

1. Install Node 22.13+ and run `npm ci`. `npm run types`, `npm run check`,
   `npm test`, `npm run build` validate the service without Google credentials.
2. Create a Workers Free application and a separate D1 database. Apply
   `migrations/0001_pairing.sql` only to that new database.
3. Create a **separate Web application OAuth client** in Google Cloud, enable
   Drive API and configure the consent screen. Keep the existing Desktop client.
   Authorized redirect URI: `https://YOUR-WORKER.workers.dev/oauth/callback`.
   Request only `https://www.googleapis.com/auth/drive.readonly`. This can read
   all Drive content accessible to the account; it is not a folder-specific scope.
4. Copy `wrangler.jsonc` to ignored `wrangler.production.jsonc`; set the real
   D1 database ID, `PUBLIC_ORIGIN`, public `GOOGLE_CLIENT_ID`, `ROOT_FOLDER_ID`.
   Keep the Free plan. No billing setup or paid upgrade is required.
5. Add Worker **secrets** `GOOGLE_CLIENT_SECRET` (Web client secret) and
   `SESSION_KEY` (32 cryptographically random bytes, base64url without padding).
   Use the dashboard Secrets UI or `wrangler secret put --config
   wrangler.production.jsonc`; never put them in source, command arguments,
   the distributable ZIP or the Miyoo. `.dev.vars.example` is fake test data.
6. Deploy with that production config. Bind the database as `DB`. Add the cron
   from the config. Disable invocation/body logging for this OAuth service:
   request URLs contain sensitive callback codes. Do not add analytics.
7. Check `/health` (protocol 1). Before secrets/config exist, login endpoints
   return 503 `service_not_configured`; this is intentional, not a working login.
8. Distribute only `config/https-setup.json` in the app, using
   `docs/https-setup.example.json`. It has the service origin, public Web client
   ID and library folder ID. It must never contain the Web client secret.

For dashboard SQL Console, remove `--` comments before flattening SQL onto one
line. Check all three tables (`pairings`, `grants`, `rate_limits`) exist.

## Protocol and security

The Miyoo creates a random 256-bit device proof and a five-minute pairing.
Only a public random pairing ID goes in the QR. The phone sees a six-digit
comparison code and signs in at Google. Secure HttpOnly SameSite cookies,
form nonce/Origin check, one-use OAuth state and PKCE S256 bind the callback.
The service verifies access to its fixed shared folder, then waits for the
device's explicit A confirmation. A QR/link must be kept private; the code is
for comparison, never the authentication secret.

The pending Google token bundle is AES-GCM encrypted in D1 with session-bound
associated data. It cannot be claimed after five minutes. Acknowledgement or
cancellation deletes it; scheduled expiry cleanup normally runs hourly, so
encrypted expired records can remain until cleanup (and Cloudflare backups
may retain older database states). This does not protect against a compromised
Worker administrator holding the encryption key.

The phone form uses `Referrer-Policy: same-origin`. Do not replace it with
`no-referrer`: browsers can then send native form POSTs with `Origin: null`,
causing the strict Origin check to reject the app's own form. Cross-site
Referer data is still suppressed; null/missing/foreign Origin, missing cookies
and invalid nonces remain rejected. This was reproduced in real Chrome and
verified through to Google's account chooser, without selecting an account.
See [the browser behavior](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Referrer-Policy).

After confirmation the Miyoo stores the real Google refresh token on its SD;
`rclone.conf` uses this service's `/v1/token` endpoint. The device has **no Web
client secret**. D1 retains only a SHA-256 refresh-token digest and grant state
to reject arbitrary token relay requests. The server receives tokens during
initial authorization and refresh in order to forward them to Google. ROMs
and library browsing still go straight from the Miyoo to Google.

Temporary service/network failures return 429/503, not `invalid_grant`.
Only Google's actual invalid grant marks that grant revoked. Automatic refresh
needs the service to remain available; Desktop setup avoids this dependency.
Revoking app access in Google can invalidate other clients in the same project.
Uninstalling the app does not revoke Google access automatically.

Limits: one active pairing per device UI, five-second status interval, 15 new
pairings per IP/day and 100 globally/day. Shared IPs count together. Free quotas
are hard availability limits, not a promise of unlimited capacity. Renewals for
the same token are limited to one per five seconds. There is no auto-upgrade.
Periodic operator review of grant counts is appropriate for this small group.

## Verification

`npm test` uses a local SQLite D1 adapter and entirely mocked Google responses.
It also runs the default deployed handler inside workerd/Miniflare with local
D1 and an intercepted outbound transport, covering exchange, folder lookup,
claim, renewal and refusal to follow redirects. No external network is used.
This runtime test catches Workers API differences that Node mocks cannot:
Workers rejects `redirect: 'error'` before sending a request. Both Google
calls use `manual` and reject non-success redirects without forwarding secrets
or bearer tokens. See [Workers redirect modes](https://developers.cloudflare.com/workers/runtime-apis/request/).
It covers first authorization, PKCE/state/cookie binding, cancellation, expiry,
claim replay/idempotence, encryption, readonly permissions, folder access,
refresh, invalid grants, rate limits and transient network errors. It does not
claim a live Google, Safari or Miyoo hardware acceptance test.

After deployment, optionally run `python ../scripts/smoke-auth-service.py
../build/https-share-profile.json`. This creates and cancels one own pairing,
checks the secure phone page and wrong-proof rejection, and uses no Google
account. It consumes one of the daily pairing allowances. The script does not
validate the Google client secret or perform an OAuth token exchange.
