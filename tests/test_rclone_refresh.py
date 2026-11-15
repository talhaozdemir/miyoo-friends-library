"""Actual rclone, fake OAuth, blocked Drive transport. No Google account/network.

The loopback proxy refuses every HTTPS CONNECT. Only the loopback token endpoint
answers requests. Tests verify refresh permission survives expiry, persistence,
process restart and fatal OAuth rejection; no browser is launched.
"""
import argparse
import configparser
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import urllib.parse

ROOT = Path(__file__).resolve().parent.parent


class Mock(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_CONNECT(self):
        # Never create an upstream socket, even if rclone tries a Google URL.
        self.server.blocked += 1
        self.send_error(502, "Test proxy: upstream access disabled")

    def do_POST(self):
        assert self.path == "/token"
        data = urllib.parse.parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode())
        assert data.get("grant_type") == ["refresh_token"]
        assert data.get("refresh_token") == ["mock-refresh-testonly"]
        self.server.refreshes += 1
        body = ({"access_token": "mock-renewed-access", "token_type": "Bearer", "expires_in": 3600}
                if self.server.mode == "ok" else {"error": self.server.mode})
        encoded = json.dumps(body).encode()
        self.send_response(200 if self.server.mode == "ok" else 400)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


def read_token(path):
    config = configparser.ConfigParser(interpolation=None)
    config.read(path)
    return json.loads(config["friends"]["token"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rclone", type=Path, default=ROOT / "downloads/rclone-v1.76.0-windows-amd64/rclone.exe")
    args = parser.parse_args()
    binary = args.rclone.resolve()
    if binary.suffix == ".exe":
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == "acd80683e23151e93bbe0d6ffa74d26e542ec510332f630beecb9944f8d1a623"
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Mock)
    server.refreshes = server.blocked = 0
    server.mode = "ok"
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    address = f"http://127.0.0.1:{server.server_port}"
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith("RCLONE_") and key.upper() not in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY")}
    env.update(HTTP_PROXY=address, HTTPS_PROXY=address, ALL_PROXY=address, NO_PROXY="")
    try:
        with tempfile.TemporaryDirectory(prefix="fl-fake-oauth-") as directory:
            path = Path(directory) / "rclone.conf"

            def expired():
                token = {"access_token": "mock-expired-access", "refresh_token": "mock-refresh-testonly",
                         "token_type": "Bearer", "expiry": "1970-01-01T00:00:01Z"}
                path.write_text("[friends]\ntype=drive\nscope=drive.readonly\nclient_id=mock.apps.googleusercontent.com\n"
                                "client_secret=mock-public-client-secret\nroot_folder_id=mock_root\n"
                                f"token_url={address}/token\ntoken={json.dumps(token)}\n", encoding="utf-8")

            def invoke():
                result = subprocess.run([str(binary), "lsjson", "friends,root_folder_id=mock_root:",
                                         "--config", str(path), "--retries", "1", "--low-level-retries", "1",
                                         "--contimeout", "1s", "--timeout", "1s", "--log-level", "ERROR"],
                                        env=env, capture_output=True, timeout=20)
                # The OAuth refresh can succeed, but Drive listing is deliberately blocked.
                assert result.returncode != 0
                return result.stderr.decode(errors="replace")

            expired()
            invoke()
            assert server.refreshes == 1 and server.blocked >= 1
            token = read_token(path)
            assert token["access_token"] == "mock-renewed-access"
            assert token["refresh_token"] == "mock-refresh-testonly"
            invoke()
            assert server.refreshes == 1, "A restarted process should reuse the saved fresh token"
            for mode in ("invalid_grant", "invalid_client"):
                server.mode = mode
                expired()
                before = server.refreshes
                error = invoke()
                assert mode in error, f"Expected OAuth category {mode}"
                # oauth2 may probe Basic vs body client authentication once.
                assert 1 <= server.refreshes - before <= 2, "Fatal OAuth error was retried excessively"
                assert read_token(path)["refresh_token"] == "mock-refresh-testonly"
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    print("Real rclone 1.76.0: expired access refreshed through loopback OAuth; refresh token persisted, reused after restart; invalid_grant/client distinguished. All upstream HTTPS blocked. No Google account used.")


if __name__ == "__main__":
    main()
