"""Start the selected Frame Finger Correction release using shared user state."""
import argparse
import fcntl
import hashlib
import importlib.util
import json
import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path
import pwd
import os
import re
import sys

sys.dont_write_bytecode = True

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
parser.add_argument('--check-home', type=Path)
args = parser.parse_args()
if args.check_home and not args.check:
    parser.error('--check-home requires --check')
home = args.check_home.resolve() if args.check_home else Path(pwd.getpwuid(os.getuid()).pw_dir)
root = home / '.local/share/frame-finger-release'
release = Path(__file__).resolve().parent
selected = json.loads((root / 'selection.json').read_text())['current']
if not re.fullmatch(r'[a-f0-9]{64}', selected) or release != root / 'releases' / selected:
    parser.error('Launcher is not in the selected release')
manifest = json.loads((release / 'payload.json').read_text())
if hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest() != selected:
    parser.error('Release manifest does not match selection')
for name in ('frame-standalone-session.py', 'frame-finger-beta-agent.py',
             'frame-finger-settings-beta', 'driver_frame_finger_standalone.so'):
    path = release / name
    if path.is_symlink():
        parser.error(f'Unexpected symlink: {name}')
    with path.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != manifest[name]:
            parser.error(f'Installed file differs: {name}')
if args.check:
    print('Frame Finger Correction: selected release verified')
    raise SystemExit(0)
state = root / 'state'
for directory in (state / 'settings-ui', state / 'scalar-audit'):
    if directory.resolve() != directory:
        parser.error('State path must not contain symlinks')
    directory.mkdir(parents=True, exist_ok=True)
with (state / 'settings-ui/session.lock').open('a') as lock:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    spec = importlib.util.spec_from_file_location('frame_session', release / 'frame-standalone-session.py')
    session = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(session)
    session.BASE = state
    handler = RotatingFileHandler(state / 'settings-ui/standalone.log',
                                  maxBytes=256 * 1024, backupCount=2)
    session.log.addHandler(handler)
    session.log.setLevel(logging.INFO)
    session.main(release)
