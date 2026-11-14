"""Build a friends bundle with public Desktop app metadata, never user tokens."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parent.parent
profile_path = ROOT / "build/friends-share-profile.json"
profile = json.loads(profile_path.read_text(encoding="utf-8"))
allowed = {"schema_version", "client_type", "root_folder_id", "client_id", "client_secret"}
if set(profile) != allowed or profile["client_type"] != "desktop_public":
    raise ValueError("Only a Desktop public client definition is allowed")
windows = ROOT / "downloads/rclone-v1.76.0-windows-amd64/rclone.exe"
if hashlib.sha256(windows.read_bytes()).hexdigest() != "acd80683e23151e93bbE0d6ffa74d26e542ec510332f630beecb9944f8d1a623".lower():
    raise ValueError("Windows rclone integrity check failed")
dest = ROOT / "dist/Miyoo-Friends-Library-Arkadas-Kurulum.zip"
def add(archive, path, name, executable=False):
    info = zipfile.ZipInfo(name)
    info.create_system = 3
    info.external_attr = (0o100755 if executable else 0o100644) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, path.read_bytes())

with zipfile.ZipFile(dest, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((ROOT / "package/App").rglob("*")):
        if not path.is_file():
            continue
        name = path.relative_to(ROOT / "package").as_posix()
        if path.name.lower() in {"rclone.conf", "settings.json"} or path.suffix == ".conf":
            raise ValueError("Account data must never be included")
        add(archive, path, name, path.name in {"launch.sh", "friends-library", "rclone"})
    for path in sorted((ROOT / "distribution/FirstSetup").glob("*")):
        if path.is_file():
            add(archive, path, "FirstSetup/" + path.name, path.suffix == ".command")
    add(archive, profile_path, "FirstSetup/setup-info.json")
    add(archive, windows, "FirstSetup/tools/windows/rclone.exe")
    for path in sorted((ROOT / "package/App/FriendsLibrary/licenses").glob("*")):
        if path.is_file():
            add(archive, path, "FirstSetup/licenses/" + path.name)

with zipfile.ZipFile(dest) as archive:
    for name in archive.namelist():
        if name.endswith(".json") or name.endswith(".conf"):
            text = archive.read(name).decode("utf-8-sig")
            if '"access_token"' in text or '"refresh_token"' in text:
                raise ValueError("Account token key detected in distributed configuration")
    actual = json.loads(archive.read("FirstSetup/setup-info.json"))
    assert set(actual) == allowed
    assert "App/FriendsLibrary/friends-library" in archive.namelist()
    assert "FirstSetup/Connect-Mac.command" in archive.namelist()
    assert archive.getinfo("FirstSetup/Connect-Mac.command").external_attr >> 16 & 0o111
digest = hashlib.sha256(dest.read_bytes()).hexdigest()
dest.with_suffix(".zip.sha256").write_text(f"{digest}  {dest.name}\n", encoding="utf-8")
print(f"Friends bundle ready: {dest.name}, {dest.stat().st_size} bytes; Desktop metadata only, no account tokens.")
