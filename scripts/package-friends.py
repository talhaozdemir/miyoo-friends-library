"""Create one validated latest installation ZIP; never distribute account tokens."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parent.parent
args = argparse.ArgumentParser()
args.add_argument('--generic', action='store_true', help='Exclude distributor profiles and computer bundle')
options = args.parse_args()
if not options.generic:
    profile_path = ROOT / "build/friends-share-profile.json"
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    allowed = {"schema_version", "client_type", "root_folder_id", "client_id", "client_secret"}
    if set(profile) != allowed or profile["client_type"] != "desktop_public":
        raise ValueError("Only a Desktop public client definition is allowed")
    if profile["schema_version"] != 1 or not re.fullmatch(r"[A-Za-z0-9_-]{1,159}", profile["root_folder_id"]):
        raise ValueError("Invalid shared folder definition")
    if not re.fullmatch(r"[A-Za-z0-9._-]+\.apps\.googleusercontent\.com", profile["client_id"]) or not re.fullmatch(r"[A-Za-z0-9._-]{1,255}", profile["client_secret"]):
        raise ValueError("Invalid public Desktop application fields")
    https_path = ROOT / "build/https-share-profile.json"
    https = json.loads(https_path.read_text(encoding="utf-8"))
    if (set(https) != {"schema_version", "service_url", "client_id", "root_folder_id"}
            or https["schema_version"] != 1
            or https["root_folder_id"] != profile["root_folder_id"]
            or not re.fullmatch(r"https://[a-z0-9.-]+\.workers\.dev", https["service_url"])
            or not re.fullmatch(r"[A-Za-z0-9._-]+\.apps\.googleusercontent\.com", https["client_id"])
            or any("REPLACE" in str(value) or "YOUR-" in str(value) for value in https.values())):
        raise ValueError("Only a configured public HTTPS profile is allowed; never include a Web client secret")
    windows = ROOT / "downloads/rclone-v1.76.0-windows-amd64/rclone.exe"
    if hashlib.sha256(windows.read_bytes()).hexdigest() != "acd80683e23151e93bbE0d6ffa74d26e542ec510332f630beecb9944f8d1a623".lower():
        raise ValueError("Windows rclone integrity check failed")

dist = ROOT / 'dist'
dist.mkdir(exist_ok=True)
dest = dist / 'Miyoo-Friends-Library-Arkadas-Kurulum.zip'
temporary = dist / (dest.name + '.tmp')
docs = {'AUTHENTICATION.md','HTTPS_SETUP.html','DEVICE_VALIDATION.md','KNOWN_LIMITATIONS.md','ROM_MAPPING.md'}
version = re.search(r'#define FL_VERSION "([^"]+)"', (ROOT/'include/fl_core.h').read_text(encoding='utf-8')).group(1)

def add(archive, path, name, executable=False):
    if path.is_symlink():
        raise ValueError('Package inputs must be regular files')
    info = zipfile.ZipInfo(name)
    info.create_system = 3
    info.external_attr = (0o100755 if executable else 0o100644) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, path.read_bytes())

try:
    with zipfile.ZipFile(temporary, 'w') as archive:
        for path in sorted((ROOT / 'package/App').rglob('*')):
            if not path.is_file():
                continue
            relative = path.relative_to(ROOT/'package/App/FriendsLibrary')
            if relative.parts[0] == 'docs' and relative.name not in docs:
                continue
            if relative.as_posix() in {'config/phone-setup.json','config/https-setup.json'}:
                continue
            name = path.relative_to(ROOT/'package').as_posix()
            if path.name.lower() in {'rclone.conf','settings.json'} or path.suffix == '.conf':
                raise ValueError('Account data must never be included')
            add(archive,path,name,path.name in {'launch.sh','friends-library','rclone'})
        if not options.generic:
            for path in sorted((ROOT/'distribution/FirstSetup').glob('*')):
                if path.is_file():
                    add(archive,path,'FirstSetup/'+path.name,path.suffix=='.command')
            add(archive,profile_path,'FirstSetup/setup-info.json')
            add(archive,https_path,'App/FriendsLibrary/config/https-setup.json')
            add(archive,windows,'FirstSetup/tools/windows/rclone.exe')
            for path in sorted((ROOT/'package/App/FriendsLibrary/licenses').glob('*')):
                if path.is_file():
                    add(archive,path,'FirstSetup/licenses/'+path.name)
        archive.writestr('App/FriendsLibrary/version.txt',version+'\n')

    with zipfile.ZipFile(temporary) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)) or archive.testzip() is not None:
            raise ValueError('Duplicate paths or ZIP integrity failure')
        for relative in ('config.json','launch.sh','friends-library','config/systems.json','icon.png','bin/rclone','certs/cacert.pem'):
            if 'App/FriendsLibrary/'+relative not in names:
                raise ValueError('Missing required app file: '+relative)
        launch=archive.read('App/FriendsLibrary/launch.sh')
        if not launch.startswith(b'#!/bin/sh\n') or b'\r' in launch:
            raise ValueError('Launcher must have a POSIX shebang and LF line endings')
        for name in names:
            if (not name.startswith(('App/FriendsLibrary/','FirstSetup/'))
                    or '..' in Path(name).parts or '\\' in name
                    or name.endswith(('rclone.conf','settings.json','PHONE_SETUP.html','phone-setup.json'))):
                raise ValueError('Unsafe or obsolete package path: '+name)
            if name.endswith(('.json','.conf')):
                text=archive.read(name).decode('utf-8-sig')
                if '"access_token"' in text or '"refresh_token"' in text:
                    raise ValueError('Account token key in distributed configuration')
        expected={
            'App/FriendsLibrary/bin/rclone':'23a2c35922b5e3bd74e57404ff82c317a360c7d83004ab9a5d1ba822d4c2d16f',
            'App/FriendsLibrary/certs/cacert.pem':'427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285',
        }
        for name,digest in expected.items():
            if hashlib.sha256(archive.read(name)).hexdigest()!=digest:
                raise ValueError('Dependency hash mismatch: '+name)
        for name in ('App/FriendsLibrary/friends-library','App/FriendsLibrary/bin/rclone'):
            data=archive.read(name)
            if data[:6]!=b'\x7fELF\x01\x01' or data[18:20]!=b'\x28\x00':
                raise ValueError('Not ARM 32-bit little endian: '+name)
            if name.endswith('friends-library') and data!=(ROOT/'build/friends-library-arm').read_bytes():
                raise ValueError('Staged application is not the current ARM build')
            if name.endswith('friends-library') and version.encode() not in data:
                raise ValueError('ARM build version does not match the source version')
            if archive.getinfo(name).external_attr>>16 & 0o111 == 0:
                raise ValueError('Executable mode missing: '+name)
        if not options.generic:
            if json.loads(archive.read('FirstSetup/setup-info.json'))!=profile:
                raise ValueError('Desktop public profile mismatch')
            if json.loads(archive.read('App/FriendsLibrary/config/https-setup.json'))!=https:
                raise ValueError('HTTPS public profile mismatch')
            if not archive.getinfo('FirstSetup/Connect-Mac.command').external_attr>>16 & 0o111:
                raise ValueError('Mac setup executable mode missing')
    # Publish only after all validation succeeds; preserve the prior ZIP on failure.
    digest=hashlib.sha256(temporary.read_bytes()).hexdigest()
    temporary.replace(dest)
    dest.with_suffix('.zip.sha256').write_text(f'{digest}  {dest.name}\n',encoding='utf-8')
    removed=[]
    for old in sorted(dist.glob('Miyoo-Friends-Library*.zip')):
        if old==dest:
            continue
        if old.is_symlink() or old.resolve().parent!=dist.resolve():
            raise ValueError('Cleanup target must stay inside dist')
        old.unlink();removed.append(old.name)
        checksum=old.with_suffix('.zip.sha256')
        if checksum.is_file():
            if checksum.is_symlink() or checksum.resolve().parent!=dist.resolve():
                raise ValueError('Cleanup checksum must stay inside dist')
            checksum.unlink();removed.append(checksum.name)
    manifest=ROOT/'build/latest-package-cleanup.json'
    history=[]
    if manifest.is_file():
        previous=json.loads(manifest.read_text(encoding='utf-8'))
        history=previous.get('runs',[previous])
    history.append({'version':version,'kept':dest.name,'removed':removed})
    manifest.write_text(json.dumps({'runs':history},indent=2)+'\n',encoding='utf-8')
    print(f'Latest {version}: {dest.name}, {dest.stat().st_size} bytes; {len(removed)} obsolete ZIP/checksum files removed.')
    print('No user tokens or Web client secret; one complete ZIP for installation and updates.')
finally:
    if temporary.is_file() and not temporary.is_symlink():
        temporary.unlink()
