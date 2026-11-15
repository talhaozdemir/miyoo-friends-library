"""Optional operator smoke check of own deployed service; no Google account used.

Creates and cancels one fresh pairing. Never prints the device proof or QR.
The normal automated suite uses mocks and does not invoke this script.
"""
import argparse
import json
import secrets
import urllib.error
import urllib.request
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("profile", type=Path, help="public https-setup.json, no credentials")
args = parser.parse_args()
profile = json.loads(args.profile.read_text(encoding="utf-8"))
origin = profile["service_url"]
assert origin.startswith("https://") and origin.endswith(".workers.dev")

def request(path, body=None):
    encoded = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(origin + path, data=encoded,
                                 headers={"Content-Type": "application/json",
                                          "User-Agent": "MiyooFriendsLibrary/0.3.0"})
    try:
        response = urllib.request.urlopen(req, timeout=25)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        return response.status, response.headers, response.read(32768)

status, headers, raw = request("/health")
assert status == 200 and json.loads(raw)["protocol"] == 1
proof = secrets.token_urlsafe(32)
status, _, raw = request("/v1/pair/create", {"device_secret": proof,
                                           "root_folder_id": profile["root_folder_id"]})
if status != 200:
    raise RuntimeError(f"Service create returned HTTP {status}: {json.loads(raw).get('error', 'unknown')}")
pair = json.loads(raw)
payload = {"pairing_id": pair["pairing_id"], "device_secret": proof}
try:
    status, _, raw = request("/v1/pair/status", payload)
    assert status == 200 and json.loads(raw)["status"] == "pending"
    status, headers, raw = request("/pair/" + pair["pairing_id"])
    assert status == 200 and pair["display_code"].encode() in raw
    assert "Secure" in headers["Set-Cookie"] and "HttpOnly" in headers["Set-Cookie"]
    assert "no-store" in headers["Cache-Control"]
    assert headers["Referrer-Policy"] == "same-origin"
    assert "form-action 'self' https://accounts.google.com;" in headers["Content-Security-Policy"]
    wrong = {**payload, "device_secret": secrets.token_urlsafe(32)}
    assert request("/v1/pair/status", wrong)[0] == 404
    assert request("/v1/pair/claim", payload)[0] == 409
finally:
    assert request("/v1/pair/cancel", payload)[0] == 200
assert request("/v1/pair/status", payload)[0] == 404
print("Live HTTPS service: health, D1 pairing, secure phone page, wrong-proof rejection, pre-approval claim rejection and cancellation passed. No Google login or token exchange performed.")
