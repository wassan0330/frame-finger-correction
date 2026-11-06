#!/usr/bin/env python3
"""Build and run the repository's self-contained tests.

Kinds:
  cpp  src/**/*-test.cpp, each built as a standalone executable with
       assertions forced on (-UNDEBUG). Pure logic; runs on any Linux g++
       (x86-64 or the Frame's ARM64).
  py   scripts/test-*.py, run with unittest. Pure logic with mocks.
  sh   scripts/test-*.sh. These need fixtures (a release archive) and are
       listed but not run unless --bootstrap-archive is given.

Nothing here touches SteamVR, the installed release or user settings.
"""
import argparse
import fnmatch
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
CXXFLAGS = ['-std=c++17', '-O0', '-g', '-Wall', '-Wextra', '-Werror', '-pthread', '-UNDEBUG']
SANITIZE = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']


def discover(kinds, patterns):
    found = []
    if 'cpp' in kinds:
        found += [('cpp', p) for p in sorted(ROOT.glob('src/**/*-test.cpp'))]
    if 'py' in kinds:
        found += [('py', p) for p in sorted(ROOT.glob('scripts/test-*.py'))]
    if 'sh' in kinds:
        found += [('sh', p) for p in sorted(ROOT.glob('scripts/test-*.sh'))]
    if patterns:
        found = [(k, p) for k, p in found if any(fnmatch.fnmatch(p.name, pat) or pat in p.name for pat in patterns)]
    return found


def run(cmd, timeout, cwd=None, env=None):
    started = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout, universal_newlines=True)
        return proc.returncode, proc.stdout, time.monotonic() - started
    except subprocess.TimeoutExpired as error:
        out = error.stdout or ''
        if isinstance(out, bytes):
            out = out.decode(errors='replace')
        return 'timeout', out, time.monotonic() - started


def test_flags(path):
    """Extra flags from a leading '// test-flags: ...' line (feature-gated headers)."""
    with path.open(encoding='utf-8') as source:
        for line in source:
            if not line.startswith('//'):
                break
            if line.startswith('// test-flags:'):
                return line.split(':', 1)[1].split()
    return []


def run_cpp(path, args, build):
    exe = build / path.stem
    flags = CXXFLAGS + test_flags(path) + (SANITIZE if args.sanitize else [])
    code, out, spent = run([args.cxx] + flags + ['-I', str(path.parent), str(path), '-o', str(exe)], args.timeout)
    if code != 0:
        return 'BUILD-FAIL', out, spent
    code, run_out, run_spent = run([str(exe)], args.timeout)
    return ('PASS' if code == 0 else 'FAIL'), out + run_out, spent + run_spent


def run_py(path, args):
    code, out, spent = run([sys.executable, '-m', 'unittest', '-q', path.stem], args.timeout,
                           cwd=str(path.parent))
    # A file without unittest cases is a plain script; run it directly.
    if code == 0 and 'Ran 0 tests' in out:
        code, out, spent = run([sys.executable, str(path)], args.timeout, cwd=str(path.parent))
    return ('PASS' if code == 0 else 'FAIL'), out, spent


def run_sh(path, args):
    if path.name == 'test-frame-release-bootstrap.sh':
        if not args.bootstrap_archive:
            return 'SKIP', 'needs --bootstrap-archive (a staged release tarball)\n', 0.0
        bootstrap = ROOT / 'scripts' / 'install-frame-release.sh'
        code, out, spent = run(['sh', str(path), str(bootstrap), args.bootstrap_archive], args.timeout)
        return ('PASS' if code == 0 else 'FAIL'), out, spent
    return 'SKIP', 'no runner rule for this shell test\n', 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('patterns', nargs='*', help='substring or glob of test file names')
    parser.add_argument('--kind', action='append', choices=['cpp', 'py', 'sh'],
                        help='limit to these kinds (repeatable; default all)')
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true', help='build C++ tests with ASan/UBSan')
    parser.add_argument('--timeout', type=float, default=120.0, help='seconds per build or run')
    parser.add_argument('--bootstrap-archive', help='release tarball for test-frame-release-bootstrap.sh')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true', help='print output of passing tests too')
    args = parser.parse_args()
    tests = discover(set(args.kind or ['cpp', 'py', 'sh']), args.patterns)
    if args.list:
        for kind, path in tests:
            print(kind, path.relative_to(ROOT).as_posix())
        return 0
    if not tests:
        print('no tests matched', file=sys.stderr)
        return 2
    if any(k == 'cpp' for k, _ in tests) and not shutil.which(args.cxx):
        print('C++ compiler not found: ' + args.cxx, file=sys.stderr)
        return 2
    counts = {}
    with tempfile.TemporaryDirectory(prefix='ffc-tests-') as build:
        for kind, path in tests:
            if kind == 'cpp':
                status, out, spent = run_cpp(path, args, Path(build))
            elif kind == 'py':
                status, out, spent = run_py(path, args)
            else:
                status, out, spent = run_sh(path, args)
            if status == 'timeout':
                status = 'TIMEOUT'
            counts[status] = counts.get(status, 0) + 1
            print('%-10s %-4s %6.1fs  %s' % (status, kind, spent, path.relative_to(ROOT).as_posix()), flush=True)
            if status not in ('PASS',) or args.verbose:
                for line in out.rstrip().splitlines():
                    print('    ' + line)
    print('summary: ' + ', '.join('%s=%d' % item for item in sorted(counts.items())))
    return 0 if set(counts) <= {'PASS', 'SKIP'} else 1


if __name__ == '__main__':
    sys.exit(main())
