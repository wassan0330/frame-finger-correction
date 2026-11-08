#!/bin/sh
# Exercise the bootstrap on Frame with a local download fixture.
set -eu
test "$#" = 2
bootstrap=$(realpath "$1")
archive=$(realpath "$2")
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/bin" "$scratch/home"
cat > "$scratch/bin/curl" <<'CURL'
#!/bin/sh
set -eu
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then cp "$TEST_ARCHIVE" "$2"; exit; fi
    shift
done
exit 2
CURL
chmod +x "$scratch/bin/curl"
export HOME="$scratch/home" PATH="$scratch/bin:$PATH" TEST_ARCHIVE="$archive"
hash=$(sha256sum "$archive" | cut -d ' ' -f 1)
url=https://example.invalid/frame-finger.tar.gz
root="$HOME/.local/share/frame-finger-release"
if sh "$bootstrap" "$url" "$(printf '%064d' 0)"; then
    echo "Bad hash unexpectedly accepted" >&2
    exit 1
fi
test ! -e "$root"
if sh "$bootstrap" "$url" "$hash" --experimental; then
    echo "Experimental installation without driver unexpectedly accepted" >&2
    exit 1
fi
test ! -e "$root"
if sh "$bootstrap" "$url" "$hash" --force; then
    echo "Unknown option unexpectedly accepted" >&2
    exit 1
fi
test ! -e "$root"
sh "$bootstrap" "$url" "$hash"
generation=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["current"])' "$root/selection.json")
python3 "$root/releases/$generation/launch-frame-release.py" --check --check-home "$HOME"
python3 "$root/releases/$generation/stage-frame-release.py" uninstall --purge
test ! -e "$root"
echo 'PASS download fixture, digest rejection, staging, launcher check and purge'
