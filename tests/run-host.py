"""Run all host regressions without requiring a PS2 or a live RA account."""
from pathlib import Path
import subprocess
import sys

for test in sorted((Path(__file__).resolve().parent / 'host').glob('test-*.py')):
    print(f'Running {test.name}', flush=True)
    subprocess.run([sys.executable, str(test)], check=True)
