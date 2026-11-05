"""Build a staging-only runtime archive from the explicit ARM64 build outputs."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
files = {
    'frame-finger-settings-beta': args.build / 'frame-finger-settings-beta',
    'driver_frame_finger_standalone.so': args.build / 'driver_frame_finger_standalone.so',
    'frame-release-dashboard': args.build / 'frame-release-dashboard',
    **{name: root / 'scripts' / name for name in (
        'frame-finger-beta-agent.py', 'frame-standalone-session.py', 'launch-frame-release.py',
        'stage-frame-release.py')},
    'LICENSE': root / 'LICENSE',
    'OPENVR-LICENSE': root / 'third_party/openvr-driver/LICENSE',
}
payload = {name: path.read_bytes() for name, path in files.items()}
for name in tuple(files)[:3]:
    data = payload[name]
    if data[:6] != b'\x7fELF\x02\x01' or data[18:20] != b'\xb7\x00':
        parser.error(f'Expected ARM64 ELF: {name}')
payload['payload.json'] = json.dumps({name: hashlib.sha256(data).hexdigest()
                                      for name, data in payload.items()}, sort_keys=True).encode()
payload['install.sh'] = b'#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\nexec python3 ./stage-frame-release.py stage "$@"\n'
with tarfile.open(args.output, 'x:gz') as archive:
    for name, data in payload.items():
        entry = tarfile.TarInfo(name)
        entry.size = len(data)
        archive.addfile(entry, io.BytesIO(data))
