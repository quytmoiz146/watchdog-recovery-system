"""Build the copied Keil sources with Arm Compiler 6 and verify isolation.
Run using Python 3. Toolchain may be supplied as the first argument.
This does not flash a board or test hardware behavior.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent
mdk = root / 'MDK-ARM'
toolchain = Path(sys.argv[1] if len(sys.argv) > 1 else r'E:\keilc\arm\ARM\ARMCLANG\bin')
output = mdk / 'sv2_da_sua'
output.mkdir(exist_ok=True)
log = []

def run(args):
    result = subprocess.run([str(a) for a in args], cwd=mdk, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.append(' '.join(str(a) for a in args) + '\n' + result.stdout)
    if result.stdout:
        print(result.stdout, end='')
    (output / 'validation_build.log').write_text('\n'.join(log), encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'Tool failed: {args[0]}, exit={result.returncode}')

# Every original file must remain byte-identical, including the user's outputs.
manifest = json.loads((root / 'original_files_sha256.json').read_text(encoding='utf-8-sig'))
for entry in manifest:
    original = root.parent / entry['Path']
    assert hashlib.sha256(original.read_bytes()).hexdigest().upper() == entry['SHA256'], original
print(f'PASS: {len(manifest)} original files unchanged.')

project = ET.parse(mdk / 'sv2_da_sua.uvprojx').getroot()
target = project.find('./Targets/Target')
assert target.findtext('TargetName') == 'sv2_da_sua'
cads = target.find('./TargetOption/TargetArmAds/Cads')
includes = [mdk / value for value in cads.findtext('./VariousControls/IncludePath').split(';')]
defines = cads.findtext('./VariousControls/Define').split(',')
assert all(p.resolve().is_relative_to(root) and p.is_dir() for p in includes)

ioc = dict(line.split('=', 1) for line in (root / 'sv2_da_sua.ioc').read_text().splitlines()
           if '=' in line and not line.startswith('#'))
assert ioc['ProjectManager.ProjectName'] == 'sv2_da_sua'
assert ioc['ProjectManager.ProjectFileName'] == 'sv2_da_sua.ioc'
assert ioc['ProjectManager.KeepUserCode'] == 'true'
assert ioc['TIM2.Prescaler'] == '7199' and ioc['TIM2.Period'] == '99'
assert ioc['IWDG.Prescaler'] == 'IWDG_PRESCALER_32' and ioc['IWDG.Reload'] == '4095'
tim = (root / 'Core/Src/tim.c').read_text()
iwdg = (root / 'Core/Src/iwdg.c').read_text()
assert re.search(r'htim2.Init.Prescaler\s*=\s*7199;', tim)
assert re.search(r'htim2.Init.Period\s*=\s*99;', tim)
assert re.search(r'hiwdg.Init.Reload\s*=\s*4095;', iwdg)
assert 'hiwdg.Init.Prescaler = IWDG_PRESCALER_32;' in iwdg
assert 72000000 / (7199 + 1) / (99 + 1) == 100
print('PASS: CubeMX/C parameters agree; TIM2=100 Hz, IWDG nominal=3.2768 s.')

objects = []
for entry in target.findall('./Groups/Group/Files/File'):
    source = (mdk / entry.findtext('FilePath').replace('\\', '/')).resolve()
    assert source.is_relative_to(root) and source.is_file(), source
    if source.suffix.lower() not in ('.c', '.s'):
        continue
    obj = output / (source.stem + '.o')
    assert obj not in objects
    objects.append(obj)
    print('Compile:', source.name)
    if source.suffix.lower() == '.c':
        args = [toolchain / 'armclang.exe', '--target=arm-arm-none-eabi',
                '-mcpu=cortex-m3', '-mthumb', '-std=c99', '-Oz', '-g',
                '-fshort-enums', '-fshort-wchar', '-ffunction-sections',
                '-Wall', '-Wextra', '-Werror', '-c', source, '-o', obj]
        args += ['-D' + d for d in defines]
        args += ['-I' + str(p) for p in includes]
    else:
        args = [toolchain / 'armasm.exe', '--cpu', 'Cortex-M3', '--apcs=interwork',
                '--debug', source, '-o', obj]
    run(args)

scatter = output / 'sv2_da_sua.sct'
scatter.write_text('''LR_IROM1 0x08000000 0x00010000 {
  ER_IROM1 0x08000000 0x00010000 {
    *.o (RESET, +First)
    *(InRoot$$Sections)
    .ANY (+RO)
    .ANY (+XO)
  }
  RW_IRAM1 0x20000000 0x00005000 {
    .ANY (+RW +ZI)
  }
}
''')
axf = output / 'sv2_da_sua.axf'
run([toolchain / 'armlink.exe', '--cpu', 'Cortex-M3', *objects,
     '--strict', '--scatter', scatter, '--summary_stderr', '--info', 'summarysizes',
     '--map', '--symbols', '--list', output / 'sv2_da_sua.map', '-o', axf])
run([toolchain / 'fromelf.exe', '--i32combined', '--output', output / 'sv2_da_sua.hex', axf])
run([toolchain / 'fromelf.exe', '--text', '-c', '--output', output / 'disassembly.txt', axf])
print(f'PASS: compiled and linked {len(objects)} source files; generated AXF/HEX.')
print('Hardware tests and CubeMX GUI regeneration have not been run.')
