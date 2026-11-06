"""Stage a Frame release without changing SteamVR registration."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile

FILES = ('frame-finger-settings-beta', 'driver_frame_finger_standalone.so', 'frame-release-dashboard',
         'LICENSE', 'OPENVR-LICENSE', 'frame-finger-beta-agent.py',
         'frame-standalone-session.py', 'launch-frame-release.py', 'stage-frame-release.py')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def activate(root, selection):
    with tempfile.NamedTemporaryFile(mode='w', dir=root, delete=False) as file:
        temporary = Path(file.name)
        try:
            json.dump(selection, file)
            file.flush()
            os.fsync(file.fileno())
            os.replace(temporary, root / 'selection.json')
        finally:
            temporary.unlink(missing_ok=True)


DESKTOP_MARKER = '# Frame Finger Correction release launcher'


def desktop_entry(home):
    path = home / '.local/share/applications/frame-finger-correction.desktop'
    if path.resolve() != path:
        raise RuntimeError('Unexpected symlink in desktop entry path')
    if path.exists() and DESKTOP_MARKER not in path.read_text().splitlines():
        raise RuntimeError('Existing desktop entry is not managed by this release')
    return path


def install_desktop(path, launcher):
    path.parent.mkdir(parents=True, exist_ok=True)
    executable = str(launcher).replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%')
    with tempfile.NamedTemporaryFile(mode='w', suffix='.desktop', dir=path.parent,
                                     delete=False) as output:
        temporary = Path(output.name)
        try:
            output.write(f'[Desktop Entry]\n{DESKTOP_MARKER}\nType=Application\n'
                         f'Name=Frame Finger Correction\nExec="{executable}"\n'
                         'Terminal=false\nCategories=Utility;\n')
            output.flush()
            subprocess.run(['desktop-file-validate', temporary], check=True, timeout=10)
            temporary.chmod(0o644)
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def dashboard(root, release, action):
    helper = root / 'releases' / release / 'frame-release-dashboard'
    manifest = root / 'dashboard.vrmanifest'
    subprocess.run([str(helper), action, str(manifest)], check=True, timeout=15,
                   env=dict(os.environ, LD_LIBRARY_PATH='/opt/steamvr/bin/linuxarm64'))


def prepare_dashboard(root):
    launcher = root / 'dashboard-launch'
    launcher.write_text("""#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec python3 -c 'import json,os,re; v=json.load(open("selection.json"))["current"]; assert re.fullmatch("[a-f0-9]{64}",v); os.execvp("python3",["python3","releases/"+v+"/launch-frame-release.py"])'
""")
    launcher.chmod(0o755)
    (root / 'dashboard.vrmanifest').write_text(json.dumps({
        'source': 'builtin', 'applications': [{
            'app_key': 'frame.finger.correction.release', 'launch_type': 'binary',
            'binary_path_linux_arm': str(launcher), 'is_dashboard_overlay': True,
            'strings': {'en_us': {'name': 'Frame Finger Correction'}},
        }],
    }))


def driver_command(root, action, release=None, experimental=False):
    runtime = Path('/opt/steamvr')
    package = root / 'frame_finger_correction'
    tool = runtime / 'bin/linuxarm64/vrpathreg'
    library = package / 'bin/linuxarm64/driver_frame_finger_correction.so'
    descriptor = package / 'driver.vrdrivermanifest'
    expected = {'name': 'frame_finger_correction', 'alwaysActivate': True, 'resourceOnly': False}
    if package.resolve() != package or library.resolve() != library or descriptor.is_symlink():
        raise RuntimeError('Unexpected symlink in release driver package')
    if action == 'unregister':
        if not descriptor.exists():
            return
        if json.loads(descriptor.read_text()) != expected:
            raise RuntimeError('Unrecognized release driver package')
        subprocess.run([tool, 'removedriver', package], check=True, timeout=15)
        library.unlink(missing_ok=True)
        descriptor.unlink()
        for directory in (library.parent, library.parent.parent, package):
            if directory.exists() and not any(directory.iterdir()):
                directory.rmdir()
        return
    source = root / 'releases' / release / 'driver_frame_finger_standalone.so'
    binary = source.read_bytes()
    pairs = (
        ('cb97844fb2f315637bf3ef3ae9620052c4f3a1e8d537a4933b5010ce11be5b72',
         '44e19eeb547a3663c8db3b77b5743cca6596c4fa5e03d6348e83320b686d1e83'),
        ('430c259a8e47ef94824462d98dd0d5bc78d0d1b0ed2f0e2c50e9562982fe06ad',
         'e54b20b9aba50b63b80a37e6b7d7b61454808dedd3f967d8c81f0c9460d18b7b'),
    )
    matched = [pair for pair in pairs if all(value.encode() in binary for value in pair)]
    if len(matched) != 1:
        raise RuntimeError('Unknown driver runtime profile')
    hashes = dict(zip(('bin/linuxarm64/vrserver', 'drivers/cv/bin/linuxarm64/driver_cv.so'), matched[0]))
    mismatches = [relative for relative, expected_hash in hashes.items()
                  if digest(runtime / relative) != expected_hash]
    if mismatches:
        ui = source.with_name('frame-finger-settings-beta')
        if (not experimental or b'FrameCompat: experimental-observation-v1' not in binary
                or b'observe-activate.txt' not in ui.read_bytes()):
            raise RuntimeError(f'Unsupported SteamVR: {mismatches[0]}; no driver registered')
        print('Experimental build installed for structural checks only; observation requires VR consent')
    if package.exists():
        raise RuntimeError('Driver already staged; uninstall before replacing the driver')
    conflict = subprocess.run([tool, 'finddriver', 'frame_scalar_audit'], capture_output=True, timeout=15)
    if conflict.returncode != 1:
        raise RuntimeError('Legacy driver present or registry unavailable; no driver registered')
    conflict = subprocess.run([tool, 'finddriver', 'frame_finger_correction'], capture_output=True, timeout=15)
    if conflict.returncode != 1:
        raise RuntimeError('Release driver already registered or registry unavailable')
    if b'/.local/share/frame-finger-release/state/' not in binary:
        raise RuntimeError('Expected a release-mode driver')
    library.parent.mkdir(parents=True)
    try:
        os.link(source, library)
        descriptor.write_text(json.dumps(expected))
        subprocess.run([tool, 'adddriver', package], check=True, timeout=15)
    except Exception:
        if descriptor.exists():
            driver_command(root, 'unregister')
        else:
            for directory in (library.parent, library.parent.parent, package):
                directory.rmdir()
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stage', 'rollback', 'uninstall'))
    parser.add_argument('--payload', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--purge', action='store_true', help='Also remove release settings, logs and metadata')
    parser.add_argument('--dashboard', action='store_true', help='Register the release dashboard in running SteamVR')
    parser.add_argument('--driver', action='store_true', help='Register the verified release driver')
    parser.add_argument('--experimental', action='store_true', help='Allow installation of the experimental build; does not start observation')
    args = parser.parse_args()
    if args.experimental and (args.action != 'stage' or not args.driver):
        parser.error('--experimental requires stage --driver')
    if args.driver and args.action != 'stage':
        parser.error('--driver requires stage')
    if args.dashboard and args.action != 'stage':
        parser.error('--dashboard requires stage')
    if args.purge and args.action != 'uninstall':
        parser.error('--purge requires uninstall')
    if platform.system() != 'Linux' or platform.machine() != 'aarch64' or os.getuid() == 0:
        parser.error('Run as a non-root Frame ARM64 user')
    root = Path.home() / '.local/share/frame-finger-release'
    if root.resolve() != root:
        parser.error('Release path must not contain symlinks')
    if args.action == 'uninstall' and not root.exists():
        print('No staged installation')
        return
    root.mkdir(parents=True, exist_ok=True)
    releases = root / 'releases'
    releases.mkdir(exist_ok=True)
    if releases.is_symlink():
        parser.error('Release directory must not be a symlink')
    with (root / 'stage.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        state = root / 'selection.json'
        selection = json.loads(state.read_text()) if state.exists() else {}
        if args.action == 'uninstall':
            session_dir = root / 'state/settings-ui'
            if session_dir.resolve() != session_dir:
                parser.error('State path must not contain symlinks')
            session_dir.mkdir(parents=True, exist_ok=True)
            with (session_dir / 'session.lock').open('a') as session_lock:
                try:
                    fcntl.flock(session_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    parser.error('Close Frame Finger Correction before uninstalling')
                owned = []
                for directory in releases.iterdir():
                    if directory.is_symlink() or not directory.is_dir() or not re.fullmatch(r'[0-9a-f]{64}', directory.name):
                        continue
                    manifest_path = directory / 'payload.json'
                    if manifest_path.is_symlink() or not manifest_path.is_file():
                        continue
                    manifest = json.loads(manifest_path.read_text())
                    identity = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
                    files = set(manifest) | {'payload.json'}
                    if (identity != directory.name or set(manifest) not in (set(FILES), set(FILES) - {'frame-release-dashboard'}, set(FILES) - {'frame-release-dashboard', 'stage-frame-release.py'})
                            or {p.name for p in directory.iterdir()} != files
                            or any((directory / n).is_symlink() or not (directory / n).is_file()
                                   or digest(directory / n) != h for n, h in manifest.items())):
                        continue
                    owned.append((directory, files))
                if selection.get('current') and selection['current'] not in {d.name for d, _ in owned}:
                    parser.error('Selected release is not a verified staged release; nothing removed')
                if args.purge:
                    expected = {'releases', 'state', 'selection.json', 'stage.lock', 'dashboard.vrmanifest', 'dashboard-launch', 'frame_finger_correction'}
                    if ({p.name for p in root.iterdir()} - expected
                            or {p for p in releases.iterdir()} != {d for d, _ in owned}):
                        parser.error('Unrecognized release files; nothing removed')
                    user_state = root / 'state'
                    if user_state.is_symlink() or not user_state.is_dir():
                        parser.error('Unexpected state directory; nothing removed')
                    # rmtree on this platform must use fd-based symlink protection.
                    if not shutil.rmtree.avoids_symlink_attacks:
                        parser.error('Safe directory removal unavailable')
                desktop = desktop_entry(Path.home())
                dashboard_path = root / 'dashboard.vrmanifest'
                if dashboard_path.exists():
                    if not selection.get('current'):
                        parser.error('No verified release available to unregister the dashboard')
                    dashboard(root, selection['current'], 'unregister')
                    dashboard_path.unlink()
                    (root / 'dashboard-launch').unlink(missing_ok=True)
                driver_command(root, 'unregister')
                desktop.unlink(missing_ok=True)
                state.unlink(missing_ok=True)
                for directory, files in owned:
                    for name in files:
                        (directory / name).unlink()
                    directory.rmdir()
                if args.purge:
                    shutil.rmtree(root / 'state')
                    releases.rmdir()
                    (root / 'stage.lock').unlink()
                    root.rmdir()
                    print('Removed staged releases, preferences, logs and metadata; SteamVR registration unchanged')
                    return
            print(f'Removed {len(owned)} staged release(s); preferences retained; SteamVR registration unchanged')
            return
        if args.action == 'rollback':
            if (root / 'frame_finger_correction').exists():
                parser.error('Uninstall the registered driver before switching generations')
            target = selection.get('previous')
            if not target or not re.fullmatch(r'[0-9a-f]{64}', target) or not (releases / target).is_dir():
                parser.error('No previous staged release')
            activate(root, {'current': target, 'previous': selection.get('current')})
            print('Restored previous staged release; SteamVR registration unchanged')
            return
        payload = args.payload
        manifest = json.loads((payload / 'payload.json').read_text())
        if set(manifest) != set(FILES):
            parser.error('Unexpected payload manifest')
        for name in FILES:
            path = payload / name
            if path.is_symlink() or not path.is_file() or digest(path) != manifest[name]:
                parser.error(f'Payload verification failed: {name}')
        release = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
        destination = releases / release
        if not destination.exists():
            with tempfile.TemporaryDirectory(dir=releases) as temporary:
                candidate = Path(temporary) / 'payload'
                candidate.mkdir()
                for name in FILES:
                    shutil.copyfile(payload / name, candidate / name)
                    if digest(candidate / name) != manifest[name]:
                        parser.error(f'Copy verification failed: {name}')
                    (candidate / name).chmod(0o755 if name in FILES[:3] else 0o644)
                (candidate / "payload.json").write_text(json.dumps(manifest, sort_keys=True))
                os.rename(candidate, destination)
        if destination.is_symlink() or any(
            (destination / name).is_symlink() or digest(destination / name) != manifest[name] for name in FILES
        ):
            parser.error('Existing staged release differs')
        if (root / 'frame_finger_correction').exists() and selection.get('current') != release:
            parser.error('Uninstall the registered driver before switching generations')
        manifest_path = root / 'dashboard.vrmanifest'
        launcher_path = root / 'dashboard-launch'
        if args.dashboard and any(path.is_symlink() for path in (manifest_path, launcher_path)):
            parser.error('Unexpected dashboard symlink')
        desktop = desktop_entry(Path.home()) if args.dashboard else None
        existed = manifest_path.exists()
        driver_added = False
        selection_changed = False
        dashboard_started = False
        try:
            if args.driver:
                driver_command(root, 'register', release, args.experimental)
                driver_added = True
            if selection.get('current') != release:
                activate(root, {'current': release, 'previous': selection.get('current')})
                selection_changed = True
            if args.dashboard:
                dashboard_started = True
                prepare_dashboard(root)
                dashboard(root, release, 'register')
                install_desktop(desktop, launcher_path)
        except Exception as error:
            if selection_changed:
                if selection:
                    activate(root, selection)
                else:
                    state.unlink(missing_ok=True)
            if dashboard_started and not existed:
                never_connected = isinstance(error, subprocess.CalledProcessError) and error.returncode == 3
                try:
                    if not never_connected and manifest_path.exists():
                        dashboard(root, release, 'unregister')
                except (OSError, subprocess.SubprocessError):
                    print('Dashboard cleanup could not be confirmed; manifest retained')
                else:
                    manifest_path.unlink(missing_ok=True)
                    launcher_path.unlink(missing_ok=True)
            if driver_added:
                driver_command(root, 'unregister')
            raise
        print(f'Staged {release}; dashboard={args.dashboard}; driver={args.driver}')



if __name__ == '__main__':
    main()
