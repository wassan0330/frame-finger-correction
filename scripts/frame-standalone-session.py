"""Frame-local UI and correction lifetime. No PC connection or PC proof relay."""
import importlib.util
import logging
from logging.handlers import RotatingFileHandler
import os
import signal
import subprocess
import time
import tempfile
from pathlib import Path

BASE = Path('/home/steamos/.local/share/frame-controller-recovery/paths-off-20261023')
log = logging.getLogger('frame-finger')
NOV_DRIVER_HASH = '430d6bfe8abd919121a26b4539fea67c7f3739458eeb5233307a0c20b96f6821'
DRIVER_HASH = 'd4cfa8f58ed56b845e143c1e0cf7206e7a4d0b7c3d7e5cd5c8986784519cd41d'


def write_issue(code):
    directory = BASE / 'settings-ui'
    fd, name = tempfile.mkstemp(prefix='.startup-issue-', dir=directory)
    try:
        with os.fdopen(fd, 'w', encoding='ascii') as output:
            output.write(code + '\n')
        os.replace(name, directory / 'startup-issue.txt')
    finally:
        if os.path.exists(name):
            os.unlink(name)


def wait_for_ui(ui, stopping):
    while ui.poll() is None and not stopping():
        time.sleep(.25)


def main(release=None):
    spec = importlib.util.spec_from_file_location('frame_agent', (release or BASE) / 'frame-finger-beta-agent.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if release:
        import json
        manifest = json.loads((release / 'payload.json').read_text())
        module.BASE = BASE
        module.LIB = release.parent.parent / 'frame_finger_correction/bin/linuxarm64/driver_frame_finger_correction.so'
    agent = module.Agent()
    agent.driver_hash = DRIVER_HASH
    agent.build_marker = 'standalone_v1'
    if release:
        agent.driver_hash = manifest['driver_frame_finger_standalone.so']
        agent.ui_hash = manifest['frame-finger-settings-beta']
        agent.ui_path = lambda: release / agent.ui_name
    trial = not release and module.LIB.is_file() and module.hashlib.sha256(module.LIB.read_bytes()).hexdigest() == NOV_DRIVER_HASH
    nov_server = '430c259a8e47ef94824462d98dd0d5bc78d0d1b0ed2f0e2c50e9562982fe06ad'
    nov_cv = 'e54b20b9aba50b63b80a37e6b7d7b61454808dedd3f967d8c81f0c9460d18b7b'
    packaged_nov = False
    if release:
        binary = (release / 'driver_frame_finger_standalone.so').read_bytes()
        packaged_nov = nov_server.encode() in binary and nov_cv.encode() in binary
    if trial:
        agent.driver_hash = NOV_DRIVER_HASH
    if trial or packaged_nov:
        agent.server_hash = nov_server
        agent.cv_hash = nov_cv
        agent.strict_mapping = True
    env = dict(os.environ, LD_LIBRARY_PATH='/opt/steamvr/bin/linuxarm64')
    stopping = False

    def stop_requested(_signal, _frame):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop_requested)
    signal.signal(signal.SIGINT, stop_requested)
    application = release / agent.ui_name if release else BASE / 'bounded-scalar-candidate' / agent.ui_name
    # Verify before executing, including in the interval before native preparation.
    import hashlib
    if application.is_symlink() or hashlib.sha256(application.read_bytes()).hexdigest() != agent.ui_hash:
        raise RuntimeError('Unknown UI executable')
    write_issue('')
    ui = subprocess.Popen([str(application), '--profile', str(BASE / 'settings-ui/preferences.txt'),
                           '--show-dashboard'] + (['--start-off'] if trial else []), env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 10
        while ui.poll() is None and not stopping and time.monotonic() < deadline:
            if agent.ui_info().get('alive'):
                break
            time.sleep(.1)
        if stopping:
            return
        if ui.poll() is not None or not agent.ui_info().get('alive'):
            raise RuntimeError(f'UI did not become ready (exit={ui.poll()})')
        try:
            agent.prepare(reuse_ui=True)
            log.info("Correction session prepared")
        except Exception as error:
            # Keep the VR screen usable, including CLOSE, rather than flash and exit.
            log.warning("Startup blocked: %s", error)
            code = 'RESTART_REQUIRED' if 'Beta not loaded' in str(error) else 'UPDATE_REQUIRED' if 'Unsupported SteamVR' in str(error) else 'START_FAILED'
            write_issue(code)
            agent.close()
            wait_for_ui(ui, lambda: stopping)
        else:
            previous = None
            while ui.poll() is None and not stopping:
                state = agent.check()
                summary = (state['attached'], state['finished'])
                if summary != previous:
                    log.info("Correction hook attached=%s restored=%s", *summary)
                    previous = summary
                if state['ui_closed'] or state['finished']:
                    break
                time.sleep(.25)
    finally:
        # Close UI first so callback expiry also protects against supervisor failure.
        if ui.poll() is None:
            ui.terminate()
            try:
                ui.wait(timeout=3)
            except subprocess.TimeoutExpired:
                ui.kill()
                ui.wait()
        agent.close()
        log.info("Frame-only session ended; correction stopped")


if __name__ == '__main__':
    handler = RotatingFileHandler(BASE / 'settings-ui/standalone.log', maxBytes=256 * 1024,
                                  backupCount=2, encoding='utf-8')
    handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
    log.addHandler(handler)
    log.setLevel(logging.INFO)
    try:
        main()
    except Exception:
        log.exception("Session failed")
        raise SystemExit(1)
