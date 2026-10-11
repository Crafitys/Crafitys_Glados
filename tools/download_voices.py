#!/usr/bin/env python3
"""Install the included voices from the complete GitHub release package."""

import hashlib
from pathlib import Path
import shutil
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
URL = "https://github.com/Crafitys/Crafitys_Glados/releases/download/v1.0.0/Crafitys_Glados.zip"
EXPECTED = {
    "en_US-glados-medium.onnx": "17ea16dd18e1bac343090b8589042b4052f1e5456d42cad8842a4f110de25095",
    "fr_FR-glados-medium.onnx": "07abe15eefacbfaa91cde1bb4996fe33590e4b2daa29a18d9423549b94287198",
}


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def install_from_archive(archive_path):
    """Read only the two named models and verify their bytes before replacing files."""
    destination = ROOT / "voices"
    destination.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive_path) as archive:
        for name, expected in EXPECTED.items():
            target = destination / name
            if target.is_file() and digest(target) == expected:
                print(f"Already installed: {name}")
                continue
            member = f"Crafitys_Glados/voices/{name}"
            with tempfile.NamedTemporaryFile(dir=destination, delete=False) as output:
                temporary = Path(output.name)
                try:
                    with archive.open(member) as source:
                        shutil.copyfileobj(source, output)
                except BaseException:
                    output.close()
                    temporary.unlink(missing_ok=True)
                    raise
            try:
                if digest(temporary) != expected:
                    raise RuntimeError(f"Voice checksum mismatch: {name}")
                temporary.replace(target)
                print(f"Installed: {name}")
            finally:
                temporary.unlink(missing_ok=True)


def main():
    if all(
        (ROOT / "voices" / name).is_file()
        and digest(ROOT / "voices" / name) == expected
        for name, expected in EXPECTED.items()
    ):
        print("Both voice models are already installed and verified.")
        return
    print("Downloading the complete v1.0.0 release (about 117 MB)...")
    with tempfile.TemporaryDirectory() as directory:
        package = Path(directory) / "Crafitys_Glados.zip"
        request = urllib.request.Request(
            URL, headers={"User-Agent": "Crafitys-Glados-Voice-Installer"}
        )
        with urllib.request.urlopen(request, timeout=120) as response, package.open(
            "wb"
        ) as output:
            shutil.copyfileobj(response, output)
        install_from_archive(package)
    print("Voice setup complete.")


if __name__ == "__main__":
    main()
