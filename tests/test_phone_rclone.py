"""Pinned rclone interactive login, fake loopback OAuth only; no real account."""
import configparser
import hashlib
import http.server
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import tempfile
import threading
import time
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parent.parent


class OAuth(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_CONNECT(self):
        self.send_error(502, "Upstream blocked")

    def do_GET(self):
        assert self.path == "/metadata?fields=id,mimeType"
        assert self.headers["Authorization"] == "Bearer test-only-access"
        body = b'{"id":"mock_root","mimeType":"application/vnd.google-apps.folder"}'
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        assert self.path == "/token"
        fields = urllib.parse.parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode())
        assert fields["grant_type"] == ["authorization_code"]
        assert fields["code"] == ["test-only-code"]
        body = json.dumps(dict(access_token="test-only-access", refresh_token="test-only-refresh",
                               token_type="Bearer", expires_in=3600,
                               scope="https://www.googleapis.com/auth/drive.readonly")).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args):
        return None


def main():
    binary = ROOT / "downloads/rclone-v1.76.0-windows-amd64/rclone.exe"
    assert hashlib.sha256(binary.read_bytes()).hexdigest() == "acd80683e23151e93bbe0d6ffa74d26e542ec510332f630beecb9944f8d1a623"
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), OAuth)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{server.server_port}"
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith("RCLONE_")
           and k.upper() not in ("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY")}
    env.update(HTTP_PROXY=base, HTTPS_PROXY=base, ALL_PROXY=base, NO_PROXY="")
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    try:
        with tempfile.TemporaryDirectory(prefix="fl-phone-oauth-") as folder:
            path = Path(folder) / "rclone.conf"
            path.write_text("[friends]\ntype=drive\nscope=drive.readonly\n"
                            "client_id=mock.apps.googleusercontent.com\nclient_secret=mock-public-secret\n"
                            "root_folder_id=mock_root\nconfig_is_local=true\nconfig_auth_no_browser=true\n"
                            f"auth_url={base}/authorize?prompt=consent\ntoken_url={base}/token\n", encoding="utf-8")
            p = subprocess.Popen([str(binary), "config", "reconnect", "friends:", "--config", str(path),
                                  "--auto-confirm", "--retries", "1", "--low-level-retries", "1"],
                                 env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True)
            messages = queue.Queue()
            def capture(pipe):
                for line in pipe:
                    messages.put(line)
            threads = [threading.Thread(target=capture, args=(pipe,), daemon=True)
                       for pipe in (p.stdout, p.stderr)]
            for thread in threads:
                thread.start()
            try:
                deadline = time.monotonic() + 15
                url = None
                while time.monotonic() < deadline and not url:
                    try:
                        match = re.search(r"http://127\.0\.0\.1:53682/auth\?state=[A-Za-z0-9_-]+", messages.get(timeout=.2))
                        if match:
                            url = match.group()
                    except queue.Empty:
                        if p.poll() is not None:
                            break
                assert url, "rclone did not expose a loopback login URL"
                try:
                    opener.open(url, timeout=3)
                    raise AssertionError("No authorization redirect")
                except urllib.error.HTTPError as response:
                    assert response.code in (302, 303, 307)
                    location = response.headers["Location"]
                assert location.startswith(base + "/authorize?")
                query = urllib.parse.parse_qs(urllib.parse.urlsplit(location).query)
                assert query["scope"] == ["https://www.googleapis.com/auth/drive.readonly"]
                assert query["access_type"] == ["offline"] and query["prompt"] == ["consent"]
                assert query["redirect_uri"] == ["http://127.0.0.1:53682/"]
                # Simulate Google's return, not a real network authorization.
                callback = query["redirect_uri"][0] + "?" + urllib.parse.urlencode(
                    dict(code="test-only-code", state=query["state"][0]))
                with opener.open(callback, timeout=3) as response:
                    assert response.status == 200
                assert p.wait(timeout=10) == 0, "rclone reconnect did not finish"
                config = configparser.ConfigParser(interpolation=None)
                config.read(path)
                token = json.loads(config["friends"]["token"])
                assert token["refresh_token"] == "test-only-refresh"
                assert config["friends"]["root_folder_id"] == "mock_root"
                assert config["friends"].get("team_drive", "") == ""
                # An actual authenticated Files.get-style request verifies the
                # folder, not a synthetic lsjson root entry with no root ID.
                verify = Path(folder) / "verify.conf"
                verify.write_text("[fl_phone_verify]\ntype=http\nno_head=true\nno_escape=true\n"
                                  "headers=Authorization,Bearer test-only-access\n"
                                  f"url={base}/\n", encoding="utf-8")
                result = subprocess.run([str(binary), "cat", "fl_phone_verify:metadata?fields=id,mimeType", "--config", str(verify),
                                         "--retries", "1", "--low-level-retries", "1", "--log-level", "ERROR"],
                                        env=env, capture_output=True, timeout=10)
                assert result.returncode == 0, "Authenticated metadata request failed"
                assert json.loads(result.stdout)["id"] == "mock_root"
                # The listener must not remain behind after the worker exits.
                try:
                    opener.open(url, timeout=.5)
                    raise AssertionError("OAuth listener survived worker")
                except urllib.error.URLError:
                    pass
            finally:
                if p.poll() is None:
                    p.kill()
                    p.wait(timeout=5)
                for thread in threads:
                    thread.join(timeout=2)
    finally:
        server.shutdown()
        server.server_close()
    print("Pinned rclone 1.76.0: first login without token, loopback callback, readonly/offline/consent, config persistence and listener cleanup passed. Fake OAuth; upstream blocked.")


if __name__ == "__main__":
    main()
