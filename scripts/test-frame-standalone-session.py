import importlib.util
import types
import unittest
from pathlib import Path
from unittest.mock import Mock, patch
spec=importlib.util.spec_from_file_location('session',Path(__file__).with_name('frame-standalone-session.py'))
session=importlib.util.module_from_spec(spec);spec.loader.exec_module(session)

class StartupTests(unittest.TestCase):
 def run_session(self, error=None):
  agent=Mock(ui_name='frame-finger-settings-beta',ui_hash='known')
  agent.ui_info.return_value={'alive':True}
  if error:agent.prepare.side_effect=RuntimeError(error)
  agent.check.return_value={'lines':[], 'ui_closed':True,'finished':False,'attached':False}
  ui=Mock();ui.poll.side_effect=[None,None,None,0] if not error else [None,None,0]
  module=types.SimpleNamespace(Agent=lambda:agent, LIB=Mock(is_file=Mock(return_value=False)))
  with patch.object(session.importlib.util,'spec_from_file_location'),patch.object(session.importlib.util,'module_from_spec',return_value=module),patch.object(session.signal,'signal'),patch.object(session.Path,'is_symlink',return_value=False),patch.object(session.Path,'read_bytes',return_value=b'ui'),patch('hashlib.sha256') as digest,patch.object(session.subprocess,'Popen',return_value=ui),patch.object(session,'write_issue') as issue,patch.object(session,'wait_for_ui') as wait:
   digest.return_value.hexdigest.return_value='known'
   session.main()
  return agent,ui,issue,wait
 def test_restart_required_keeps_ui_until_user_close(self):
  agent,ui,issue,wait=self.run_session('Beta not loaded; normal Frame SteamVR restart required once after installation')
  issue.assert_any_call('RESTART_REQUIRED');wait.assert_called_once();agent.check.assert_not_called();ui.terminate.assert_not_called()
 def test_other_start_error_also_keeps_ui(self):
  agent,ui,issue,wait=self.run_session('Unknown runtime/candidate; no request sent')
  issue.assert_any_call('START_FAILED');wait.assert_called_once();agent.check.assert_not_called()
 def test_normal_close_releases_session(self):
  agent,ui,issue,wait=self.run_session()
  wait.assert_not_called();agent.close.assert_called_once();agent.prepare.assert_called_once_with(reuse_ui=True)
if __name__=='__main__':unittest.main()
