import importlib.util,stat,types,unittest
from pathlib import Path
from unittest.mock import Mock,MagicMock,patch
spec=importlib.util.spec_from_file_location('agent',Path(__file__).with_name('frame-finger-beta-agent.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class UiIdentityTests(unittest.TestCase):
 def probe(self,stamp=1000,exe=None,now=1000):
  app=str(module.BASE/'bounded-scalar-candidate/frame-finger-settings-beta')
  profile=str(module.BASE/'settings-ui/preferences.txt')
  entry=MagicMock();entry.name='123';entry.stat.return_value=types.SimpleNamespace(st_uid=42)
  entry.__truediv__.return_value.read_bytes.return_value=((exe or app)+'\0--profile\0'+profile+'\0').encode()
  proc=Mock();proc.iterdir.return_value=[entry]
  with patch.object(module.os,'getuid',return_value=42,create=True),patch.object(module.os,'O_NOFOLLOW',0x20000,create=True),patch.object(module.os,'open',return_value=99),patch.object(module.os,'fstat',return_value=types.SimpleNamespace(st_mode=stat.S_IFREG|0o600,st_uid=42,st_size=len(f'FRAME_UI_HEARTBEAT_1 1 2 {stamp} 1'))),patch.object(module.os,'read',return_value=f'FRAME_UI_HEARTBEAT_1 1 2 {stamp} 1'.encode()),patch.object(module.os,'close'),patch.object(module,'Path',return_value=proc),patch.object(module.time,'monotonic_ns',return_value=now*1000000):
   agent=module.Agent();agent.write_request=Mock();result=agent.ui_info();agent.write_request.assert_not_called();return result
 def test_exact_live_ui(self):self.assertTrue(self.probe()['alive'])
 def test_stale_ui(self):self.assertFalse(self.probe(stamp=1000,now=2500)['alive'])
 def test_other_process(self):self.assertFalse(self.probe(exe='/unrelated/application')['alive'])
 def test_future_timestamp(self):self.assertFalse(self.probe(stamp=1001)['alive'])
if __name__=='__main__':unittest.main()
