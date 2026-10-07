#!/usr/bin/env python3
"""Run the application's screenshot smoke check in an isolated X11 server."""
import argparse
import os
from pathlib import Path
import select
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--language", default="en", choices=("en", "zh_CN", "ja", "ko", "fr", "de", "es"))
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
xvfb = shutil.which('Xvfb') or str(root / '.deps/qt/usr/bin/Xvfb')
env = os.environ.copy()
env["PHOTOSHIP_TESTING"] = "1"
local_libs = root / '.deps/qt/usr/lib/x86_64-linux-gnu'
if local_libs.exists():
    env['LD_LIBRARY_PATH'] = str(local_libs) + ':' + env.get('LD_LIBRARY_PATH', '')
(root / 'dist').mkdir(exist_ok=True)
with (root / 'dist/x11-server.log').open('w') as log:
    server = subprocess.Popen(
        [xvfb, '-displayfd', '1', '-screen', '0', '1600x1000x24', '-nolisten', 'tcp'],
        stdout=subprocess.PIPE, stderr=log, env=env, text=True)
    try:
        ready, _, _ = select.select([server.stdout], [], [], 10)
        display = server.stdout.readline().strip() if ready else ''
        if not display.isdigit():
            raise SystemExit('Virtual display could not start; see dist/x11-server.log')
        env['DISPLAY'] = ':' + display
        env['QT_QPA_PLATFORM'] = 'xcb'
        subprocess.run([str(root / 'scripts/run.sh'), '--language', args.language, '--screenshot', str(root / 'dist/preview-x11.png')],
                       cwd=root, env=env, check=True, timeout=20)
        print('X11 Qt window smoke check passed')
    finally:
        server.terminate()
        server.wait(timeout=5)
