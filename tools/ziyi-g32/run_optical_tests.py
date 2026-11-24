"""Build/run the actual CPU optical serializer+renderer with sanitizers."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess

P = Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--cc', default='cc')
ap.add_argument('--fixture', type=Path, required=True)
args = ap.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=False)
fixture = args.fixture.resolve()
env = dict(os.environ)
if Path(args.cc).is_file():
    env['PATH'] = str(Path(args.cc).resolve().parent) + os.pathsep + env['PATH']
env.update(ASAN_OPTIONS='abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
command = [args.cc, '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
           '-fsanitize=address,undefined', str(P/'files/ziyi_g32_optical.c'),
           str(P/'test_optical.c'), '-o', str(out/'optical.exe')]
q = subprocess.run(command, capture_output=True, text=True, env=env)
(out/'compile.log').write_text(q.stdout+q.stderr)
if q.returncode:
    raise RuntimeError(q.stderr)
q = subprocess.run([str(out/'optical.exe'), str(fixture), str(out)], capture_output=True, text=True, env=env)
(out/'run.log').write_text(q.stdout+q.stderr)
if q.returncode or 'G32_OPTICAL_HOST_PASS' not in q.stdout:
    raise RuntimeError(q.stdout+q.stderr)
inputs = [P/'files/ziyi_g32_optical.c', P/'files/ziyi_g32_optical.h', P/'test_optical.c', Path(__file__), fixture]
(out/'audit.json').write_text(json.dumps(dict(result='G32_OPTICAL_ASAN_UBSAN_HOST_PASS',
    syntheticOnly=True, phoneOperated=False, GPURegistersAccessed=False,
    inputs=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in inputs]), indent=2)+'\n')
print(q.stdout.strip())
