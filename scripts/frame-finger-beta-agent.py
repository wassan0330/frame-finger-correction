"""Persistent all8-finger beta application helper. Native UI controls correction.
No ON command is generated here. Importing and --help have no side effects.
"""
import hashlib,json,os,secrets,subprocess,sys,tempfile,time
from pathlib import Path
BASE=Path('/home/steamos/.local/share/frame-controller-recovery/paths-off-20261023')
LIB=BASE/'scalar-audit/package/frame_scalar_audit/bin/linuxarm64/driver_frame_scalar_audit.so'
HASH='99fab5b65f9b3a381d202a852c956ff6af5ef64b42142ec65b112f9b0c6575d4'
class Agent:
 driver_hash=HASH
 ui_name="frame-finger-settings-beta"
 ui_hash="2bd98f49be3f3552519aafc73b53338cb225144255d94b6b40b5f11b9b108290"
 build_marker="all8_v1"
 server_hash='cb97844fb2f315637bf3ef3ae9620052c4f3a1e8d537a4933b5010ce11be5b72'
 cv_hash='44e19eeb547a3663c8db3b77b5743cca6596c4fa5e03d6348e83320b686d1e83'
 strict_mapping=False
 def ui_path(self):return BASE/'bounded-scalar-candidate'/self.ui_name
 def __init__(self):self.log=None;self.attached=False;self.started=False;self.finished=False;self.lock=None;self.until=0
 def write_request(self,seconds):
  directory=BASE/'scalar-audit'
  fd,name=tempfile.mkstemp(prefix='.supervised-request-',dir=directory)
  try:
   with os.fdopen(fd,'w',encoding='ascii') as stream:
    stream.write(f'FRAME_SCALAR_OFF_PROBE_1 {int(time.time())} {secrets.token_hex(8)} {seconds}\n');stream.flush();os.fsync(stream.fileno())
   os.replace(name,directory/'off-vtable-request')
  finally:
   if os.path.exists(name):os.unlink(name)
 def ui_info(self):
  import stat
  try:
   fd=os.open(BASE/'settings-ui/ui-heartbeat.txt',os.O_RDONLY|os.O_NOFOLLOW)
   try:
    info=os.fstat(fd)
    if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or info.st_mode&0o022 or not 0<info.st_size<=160:return {'alive':False}
    fields=os.read(fd,161).decode('ascii').split()
   finally:os.close(fd)
   if len(fields)!=5 or fields[0]!='FRAME_UI_HEARTBEAT_1':return {'alive':False}
   session,sequence,stamp,revision=map(int,fields[1:])
   if min(session,sequence,stamp,revision)<=0 or not 0<=time.monotonic_ns()//1000000-stamp<1500:return {'alive':False}
   expected=str(self.ui_path())
   profile=str(BASE/'settings-ui/preferences.txt')
   for entry in Path('/proc').iterdir():
    if not entry.name.isdigit():continue
    try:
     if entry.stat().st_uid!=os.getuid():continue
     args=[part.decode() for part in (entry/'cmdline').read_bytes().split(b'\0') if part]
     if args and args[0]==expected and '--profile' in args and args[args.index('--profile')+1]==profile:
      return {'alive':True,'session':session,'pid':int(entry.name)}
    except (OSError,ValueError,IndexError,UnicodeError):continue
  except (OSError,ValueError,UnicodeError):pass
  return {'alive':False}
 def prepare(self,reuse_ui=False):
  if self.log is not None:raise RuntimeError('Preparation already attempted')
  import fcntl
  self.lock=(BASE/'scalar-audit/off-vtable-control.lock').open('a');fcntl.flock(self.lock.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
  server=Path('/opt/steamvr/bin/linuxarm64/vrserver')
  cv=Path('/opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so')
  paths={LIB:self.driver_hash,self.ui_path():self.ui_hash,server:self.server_hash,cv:self.cv_hash}
  for path,digest in paths.items():
   if path.is_symlink():raise RuntimeError(f'Unexpected symlink: {path}')
   with path.open('rb') as stream:actual=hashlib.file_digest(stream,'sha256').hexdigest()
   if actual!=digest:
    kind='Unsupported SteamVR build' if path.is_relative_to('/opt/steamvr') else 'Unknown app binary'
    raise RuntimeError(f'{kind}: {path.name} sha256={actual}; correction OFF')
  pids=subprocess.check_output(['pgrep','-x','vrserver'],text=True).split()
  if len(pids)!=1:raise RuntimeError('Expected one vrserver')
  inode=LIB.stat().st_ino
  mapped=False
  for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines():
   fields=line.split()
   if len(fields)==6 and fields[5]==str(LIB) and int(fields[4])==inode:mapped=True
  if self.strict_mapping:
   with Path(f'/proc/{pids[0]}/exe').open('rb') as stream:
    if hashlib.file_digest(stream,'sha256').hexdigest()!=self.server_hash:raise RuntimeError('Unsupported SteamVR running executable')
   # Overlay mounts can report the backing device in maps and the virtual
   # device in stat. Verify the process-root file and exact non-deleted inode.
   process_cv=Path(f'/proc/{pids[0]}/root') / str(cv).lstrip('/')
   with process_cv.open('rb') as stream:
    cv_inode=os.fstat(stream.fileno()).st_ino
    if hashlib.file_digest(stream,'sha256').hexdigest()!=self.cv_hash:
     raise RuntimeError('Unsupported SteamVR process-root cv image')
   if not any(len(fields)==6 and fields[5]==str(cv) and 'x' in fields[1] and int(fields[4])==cv_inode
              for fields in (line.split() for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines())):
    raise RuntimeError('Unsupported SteamVR mapped cv image')
  if not mapped and not self.strict_mapping:
   # Disk restoration/replacement need not unload the resident NODELETE module.
   # Require this process's explicit build marker as well as a mapped library.
   marker=f'FrameFingerBeta: build={self.build_marker} pid={pids[0]};'
   logtext=Path('/home/steamos/.local/share/Steam/logs/vrserver.txt').read_text(encoding='utf-8',errors='replace')
   mapped=marker in logtext and any(str(LIB) in line for line in Path(f'/proc/{pids[0]}/maps').read_text().splitlines())
  if not mapped:raise RuntimeError('Beta not loaded; normal Frame SteamVR restart required once after installation')
  ui=BASE/'settings-ui'
  for name in ('beta-learn.txt','beta-status.txt'):
   path=ui/name
   if path.is_symlink():raise RuntimeError('Unexpected symlink; no request sent')
   if path.exists():
    if path.stat().st_uid!=os.getuid():raise RuntimeError('Unexpected owner')
    path.unlink()
  if reuse_ui:
   if not self.ui_info().get('alive'):raise RuntimeError('Headset UI is not active')
   ui_message='Existing headset UI retained; no restart'
  else:
   response=subprocess.run(['python3',str(BASE/'reopen-frame-settings-ui.py'),'--beta'],capture_output=True,text=True,timeout=6)
   if response.returncode:raise RuntimeError(response.stdout+response.stderr)
   ui_message=response.stdout
  self.log=Path('/home/steamos/.local/share/Steam/logs/vrserver.txt').open(encoding='utf-8',errors='replace');self.log.seek(0,os.SEEK_END)
  self.write_request(60);self.until=0
  return {'prepared':True,'ui_controls_correction':True,'time_limit':None,'ui':ui_message}
 def check(self):
  if self.log is None:raise RuntimeError('Not prepared')
  lines=[]
  while True:
   line=self.log.readline()
   if not line:break
   if not any(tag in line for tag in ('FrameFingerBeta:','FrameScalarOffProbe:','FrameContinuousShadow:')):continue
   lines.append(line.strip())
   if 'attached UpdateScalar slot only' in line:self.attached=True
   if 'original slot restored' in line:self.attached=False;self.finished=True
   if 'FrameFingerBeta: on=1' in line:self.started=True
  ui_closed=False
  try:
   heartbeat=(BASE/'settings-ui/ui-heartbeat.txt').read_text(encoding='ascii').split()
   ui_closed=len(heartbeat)!=5 or heartbeat[0]!='FRAME_UI_HEARTBEAT_1' or int(heartbeat[1])==0 or not 0<=time.monotonic_ns()//1000000-int(heartbeat[3])<1500
  except (OSError,ValueError):ui_closed=True
  return {'ui_closed':ui_closed,'lines':lines,'attached':self.attached,'started':self.started,'finished':self.finished,'timed_out':False}
 def stop(self):
  if self.log is None:return {'stopped':True,'never_prepared':True}
  self.write_request(0);deadline=time.monotonic()+4;lines=[]
  while time.monotonic()<deadline:
   state=self.check();lines.extend(state['lines'])
   if self.finished:return {'stopped':True,'lines':lines}
   time.sleep(.05)
  raise RuntimeError('Original slot restoration not confirmed')
 def close(self):
  if self.log:self.stop();self.log.close();self.log=None
  if self.lock:self.lock.close();self.lock=None

def main():
 if '--help' in sys.argv:print(__doc__);return
 agent=Agent()
 try:
  for line in sys.stdin:
   try:
    command=json.loads(line);action=command['action']
    if action=='prepare':result=agent.prepare(reuse_ui=command.get('reuse_ui',False) is True)
    elif action=='ui_info':result=agent.ui_info()
    elif action=='launch_ui':
     with (BASE/'settings-ui/headset-launch.log').open('a') as launch_log:
      process=subprocess.Popen([str(Path.home()/'.local/bin/frame-finger-correction')],stdin=subprocess.DEVNULL,stdout=launch_log,stderr=launch_log,start_new_session=True)
     result={'launched':True,'pid':process.pid}
    elif action=='check':result=agent.check()
    elif action=='stop':result=agent.stop()
    else:raise ValueError('Unknown action')
    print(json.dumps({'ok':True,'result':result}),flush=True)
   except Exception as error:print(json.dumps({'ok':False,'error':str(error)}),flush=True)
 finally:agent.close()
if __name__=='__main__':main()
