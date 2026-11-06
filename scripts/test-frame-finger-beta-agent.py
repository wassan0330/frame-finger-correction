import importlib.util,io,unittest
from pathlib import Path
from unittest.mock import Mock,patch
spec=importlib.util.spec_from_file_location('agent',Path(__file__).with_name('frame-finger-beta-agent.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class AgentTests(unittest.TestCase):
 def test_no_preparation_no_command(self):
  agent=module.Agent();agent.write_request=Mock()
  self.assertTrue(agent.stop()['never_prepared']);agent.close();agent.write_request.assert_not_called()
 def test_observation_and_restore_ack(self):
  agent=module.Agent();agent.write_request=Mock();agent.until=1000
  agent.log=io.StringIO('FrameScalarOffProbe: attached UpdateScalar slot only\nFrameFingerBeta: on=1 present=255 witnessed=255 learned=0\nFrameScalarOffProbe: original slot restored\n')
  state=agent.check();self.assertTrue(state['started']);self.assertTrue(state['finished']);self.assertFalse(state['attached'])
  self.assertTrue(agent.stop()['stopped']);agent.write_request.assert_called_once_with(0)
 def test_missing_restore_is_error(self):
  agent=module.Agent();agent.write_request=Mock();agent.log=io.StringIO('');agent.until=1000
  with patch.object(module.time,'monotonic',side_effect=[0,0,0,5]),patch.object(module.time,'sleep'):
   with self.assertRaises(RuntimeError):agent.stop()
  agent.write_request.assert_called_once_with(0)
if __name__=='__main__':unittest.main()
