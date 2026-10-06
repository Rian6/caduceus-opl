"""Preprocess real boot/reset sources: default achievements never install GS traps."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[2]
features = (root / 'opl/modules/network/common/ra_features.h').read_text()
for name in ('main', 'padhook'):
    source = (root / f'opl/ee_core/src/{name}.c').read_text()
    source = re.sub(r'^#include .*$', '', source, flags=re.M)
    preprocessed = subprocess.check_output(
        ['gcc', '-E', '-P', '-x', 'c', '-'], input=features+source, text=True)
    assert not re.search(r'\b(?:Enable|Disable)GSTracker\s*\(', preprocessed), name
    assert re.search(r'\b(?:Enable|Disable)GSM\s*\(', preprocessed), name
    if name == 'main':
        assert 'RA_SetupWatchList();' in preprocessed
        assert 'Install_Kernel_Hooks();' in preprocessed
print('PASS: default boot/reset has no RA graphics trap; configured GSM and telemetry retained')
