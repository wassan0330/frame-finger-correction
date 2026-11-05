#!/bin/sh
# Bootstrap for a pinned Frame Finger Correction release archive.
set -eu
if [ "$#" -lt 2 ] || [ "$#" -gt 4 ]; then
    echo "Usage: sh install-frame-release.sh <release-archive-https-url> <sha256> [--dashboard] [--driver]" >&2
    exit 2
fi
for option in "${3:-}" "${4:-}"; do
    case "$option" in ""|--dashboard|--driver) ;; *) echo "Unknown option: $option" >&2; exit 2;; esac
done
case "$1" in https://*) ;; *) echo "HTTPS required" >&2; exit 2;; esac
case "$(uname -s):$(uname -m)" in Linux:aarch64) ;; *) echo "Steam Frame ARM64 required" >&2; exit 2;; esac
[ "$(id -u)" -ne 0 ] || { echo "Run as the Frame user, without sudo" >&2; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
trap 'exit 1' HUP INT TERM
curl --fail --silent --show-error --location --proto '=https' --proto-redir '=https' \
    --connect-timeout 10 --max-time 180 --max-filesize 67108864 -o "$tmp/release.tar.gz" -- "$1"
python3 - "$tmp" "$2" <<'PY'
import hashlib
import re
import sys
import tarfile
from pathlib import Path, PurePosixPath

directory, expected = Path(sys.argv[1]), sys.argv[2]
archive = directory / "release.tar.gz"
with archive.open("rb") as stream:
    actual = hashlib.file_digest(stream, "sha256").hexdigest()
if not re.fullmatch(r"[0-9a-f]{64}", expected) or actual != expected:
    sys.exit("Release SHA-256 mismatch")
output = directory / "payload"
output.mkdir()
with tarfile.open(archive, "r:gz") as tar:
    seen = set()
    size = 0
    for item in tar:
        path = PurePosixPath(item.name)
        if (not path.parts or path.is_absolute() or ".." in path.parts
                or path in seen or not (item.isfile() or item.isdir()) or item.issparse()):
            sys.exit("Unsafe release archive")
        seen.add(path)
        size += item.size
        if len(seen) > 256 or size > 128 * 1024 * 1024:
            sys.exit("Release archive exceeds limits")
        target = output.joinpath(*path.parts)
        if item.isdir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            import shutil
            target.parent.mkdir(parents=True, exist_ok=True)
            with tar.extractfile(item) as source, target.open("xb") as destination:
                shutil.copyfileobj(source, destination)
if not (output / "install.sh").is_file():
    sys.exit("Release installer missing")
PY
cd "$tmp/payload"
shift 2
sh ./install.sh "$@"
