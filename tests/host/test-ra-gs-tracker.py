"""Execute breakpoint setup branches; assert RA excludes reads and GS status."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = (root / 'opl/ee_core/src/gsm_engine.S').read_text()
source = source.split('Enable_GSBreakpoint:', 1)[1].split('.end Enable_GSBreakpoint', 1)[0]
constants = dict((name, int(value, 0)) for name, value in re.findall(
    r'\.equ\s+(TRAP_BASE|TRAP_MASK),\s+(0x[0-9A-Fa-f]+)',
    (root / 'opl/ee_core/include/gsm_defines.h').read_text()))
instructions, labels = [], {}
for line in source.splitlines():
    line = line.split('#')[0].strip()
    if not line:
        continue
    if line.endswith(':'):
        labels[line[:-1]] = len(instructions)
    else:
        instructions.append(line)

def setup(track_only):
    regs, special, pc = {}, {}, 0
    while pc < len(instructions):
        parts = re.split(r'[\s,]+', instructions[pc])
        op, args = parts[0], parts[1:]
        pc += 1
        if op == 'li':
            regs[args[0]] = constants[args[1]] if args[1] in constants else int(args[1], 0)
        elif op == 'la':
            assert args[1] == 'GSMFlags'
            regs[args[0]] = 1
        elif op == 'lbu':
            assert args[1] == 'TRACK_ONLY($t0)'
            regs[args[0]] = track_only
        elif op == 'beqz':
            assert instructions[pc] == 'nop', 'branch delay must have no side effects'
            if not regs[args[0]]:
                pc = labels[args[1]]
        elif op in ('mtbpc', 'mtdab', 'mtdabm'):
            special[op] = regs[args[0]]
        elif op == 'mfbpc':
            regs[args[0]] = special['mtbpc']
        elif op == 'jr':
            assert args == ['$ra']
            break
        else:
            assert op in ('nop', 'sync.l', 'sync.p'), op
    return special

normal, passive = setup(0), setup(1)
assert normal == {'mtbpc': 0x60280000, 'mtdab': 0x12000000, 'mtdabm': 0x1FFFEF0F}
assert passive['mtbpc'] == 0x20280000
assert not passive['mtbpc'] & 0x40000000  # DRE
assert passive['mtbpc'] & 0x20000000  # DWE
def matches(address):
    return (address & passive['mtdabm']) == (passive['mtdab'] & passive['mtdabm'])
for segment in (0, 0x80000000, 0xA0000000):
    for offset in (0, 0x70, 0x80, 0x90, 0xA0):
        assert matches(segment | (0x12000000 + offset))
    for offset in (0x1000, 0x1010, 0x1040, 0x1080):
        assert not matches(segment | (0x12000000 + offset))
assert not matches(0x10003020)  # GIF_STAT
print('PASS: passive writes only; CSR/IMR excluded in all segments; normal GSM preserved')
