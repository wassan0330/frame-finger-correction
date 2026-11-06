"""Check the installer distinguishes experimental registration from authorization."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('stage', Path(__file__).with_name('stage-frame-release.py'))
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)


class ExperimentalRegistrationTests(unittest.TestCase):
    def test_unknown_runtime_requires_opt_in_and_both_capability_markers(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            release = root / 'releases' / 'test'
            release.mkdir(parents=True)
            driver = release / 'driver_frame_finger_standalone.so'
            driver.write_bytes(
                b'cb97844fb2f315637bf3ef3ae9620052c4f3a1e8d537a4933b5010ce11be5b72 '
                b'44e19eeb547a3663c8db3b77b5743cca6596c4fa5e03d6348e83320b686d1e83 '
                b'/.local/share/frame-finger-release/state/ FrameCompat: experimental-observation-v1')
            ui = release / 'frame-finger-settings-beta'
            ui.write_bytes(b'ordinary UI')
            with patch.object(stage, 'digest', return_value='unknown'), patch.object(stage.subprocess, 'run') as run:
                with self.assertRaisesRegex(RuntimeError, 'Unsupported SteamVR'):
                    stage.driver_command(root, 'register', 'test')
                with self.assertRaisesRegex(RuntimeError, 'Unsupported SteamVR'):
                    stage.driver_command(root, 'register', 'test', experimental=True)
                run.assert_not_called()
                ui.write_bytes(b'observe-activate.txt')
                run.side_effect = [subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 1),
                                   subprocess.CompletedProcess([], 0)]
                stage.driver_command(root, 'register', 'test', experimental=True)
                self.assertEqual([call.args[0][1] for call in run.call_args_list],
                                 ['finddriver', 'finddriver', 'adddriver'])


if __name__ == '__main__':
    unittest.main()
