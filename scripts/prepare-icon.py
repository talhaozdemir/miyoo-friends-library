"""Prepare the generated source as a small, padded Onion Apps menu asset."""
from pathlib import Path
import hashlib
import zipfile
from PIL import Image

root = Path(__file__).resolve().parent.parent
source = Image.open(root / "assets/friends-library-icon.png").convert("RGBA")
bounds = source.getbbox()
if not bounds:
    raise ValueError("Icon source is fully transparent")
content = source.crop(bounds)
content.thumbnail((52, 52), Image.Resampling.LANCZOS)
icon = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
icon.alpha_composite(content, ((64-content.width)//2, (64-content.height)//2))
target = root / "package/App/FriendsLibrary/icon.png"
icon.save(target, optimize=True)
print(f"Prepared {target}: canvas 64x64, content {content.width}x{content.height}")
archive = root / "dist/Miyoo-Friends-Library-v0.1.3-icon-small.zip"
archive.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
    bundle.write(target, "App/FriendsLibrary/icon.png")
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
print(f"Icon-only update: {archive}")
