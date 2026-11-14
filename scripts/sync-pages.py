"""Copy the static information pages to GitHub Pages' main /docs source."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
for name in ("index.html", "privacy.html", "terms.html", "style.css"):
    shutil.copyfile(root / "sites/oauth-info/dist" / name, root / "docs" / name)
(root / "docs/.nojekyll").write_text("", encoding="utf-8")
print("GitHub Pages files synchronized: main /docs; no account configuration copied.")
