"""Latest-only packaging integration using fake old releases and real build inputs."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='mfl-release-') as folder:
    root=Path(folder)
    for relative in ('scripts/package-friends.py','include/fl_core.h','build/friends-library-arm'):
        target=root/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/relative,target)
    shutil.copytree(ROOT/'package/App',root/'package/App')
    dist=root/'dist';dist.mkdir()
    old=dist/'Miyoo-Friends-Library-v0.1.0-source.zip';old.write_bytes(b'FAKE_OLD_RELEASE')
    old.with_suffix('.zip.sha256').write_text('FAKE_OLD_HASH',encoding='utf-8')
    unrelated=dist/'other-project.zip';unrelated.write_bytes(b'KEEP_OTHER_PROJECT')
    run=subprocess.run([sys.executable,str(root/'scripts/package-friends.py'),'--generic'],capture_output=True,text=True)
    assert run.returncode==0,run.stdout+run.stderr
    latest=dist/'Miyoo-Friends-Library-Arkadas-Kurulum.zip'
    assert latest.is_file() and not old.exists() and not old.with_suffix('.zip.sha256').exists()
    assert unrelated.read_bytes()==b'KEEP_OTHER_PROJECT'
    with zipfile.ZipFile(latest) as archive:
        assert not any(name.endswith(('PHONE_SETUP.html','phone-setup.json')) for name in archive.namelist())
        assert archive.read('App/FriendsLibrary/version.txt').strip()==b'0.3.2'
    previous=hashlib.sha256(latest.read_bytes()).hexdigest()
    old.write_bytes(b'KEEP_IF_NEW_BUILD_FAILS')
    (root/'package/App/FriendsLibrary/forbidden.conf').write_text('MOCK_CREDENTIAL',encoding='utf-8')
    failed=subprocess.run([sys.executable,str(root/'scripts/package-friends.py'),'--generic'],capture_output=True,text=True)
    assert failed.returncode!=0
    assert hashlib.sha256(latest.read_bytes()).hexdigest()==previous
    assert old.read_bytes()==b'KEEP_IF_NEW_BUILD_FAILS'
    assert not (dist/(latest.name+'.tmp')).exists()
    (root/'package/App/FriendsLibrary/forbidden.conf').unlink()
    launcher=root/'package/App/FriendsLibrary/launch.sh'
    launcher.write_bytes(launcher.read_bytes().replace(b'\n',b'\r\n'))
    failed=subprocess.run([sys.executable,str(root/'scripts/package-friends.py'),'--generic'],capture_output=True,text=True)
    assert failed.returncode!=0 and 'Launcher must have' in failed.stderr
    assert hashlib.sha256(latest.read_bytes()).hexdigest()==previous
    assert old.read_bytes()==b'KEEP_IF_NEW_BUILD_FAILS'
print('Release integration passed: one latest app ZIP, obsolete checksum cleanup, unrelated archives preserved, failed rebuild preserves last valid release.')
