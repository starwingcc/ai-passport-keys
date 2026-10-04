#!/usr/bin/env python3
"""Generate reproducible LVGL fonts from a licensed CJK source (fontTools required)."""
import argparse
from pathlib import Path
import subprocess
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--converter', type=Path, required=True)
p.add_argument('--node', default='node')
a = p.parse_args()
# All printable ASCII plus every non-ASCII character used by the device sources.
text = ''.join((ROOT / 'main/main.c').read_text())
for path in sorted((ROOT / 'main/keyboard').glob('*.c')):
    if path.name not in ('keypad_web.c',):
        text += path.read_text()
chars = ''.join(chr(i) for i in sorted(set(range(32, 127)) | {ord(c) for c in text if ord(c) >= 128}))
font = TTFont(a.source, recalcTimestamp=False)
missing = set(map(ord, chars)) - set(font.getBestCmap())
if missing:
    raise SystemExit('Missing source glyphs: ' + str(sorted(missing)))
options = subset.Options()
options.name_IDs = ['*']
sub = subset.Subsetter(options=options)
sub.populate(text=chars)
sub.subset(font)
# Rename the modified subset instead of distributing it under the reserved family.
for record in font['name'].names:
    if record.nameID in (1, 3, 4, 6, 16):
        record.string = 'PassportKeysSubset'.encode(record.getEncoding())
asset = ROOT / 'assets/fonts/passport-keys-subset.otf'
font.save(asset)
(ROOT / 'assets/fonts/passport-codepoints.txt').write_text(''.join(f'U+{ord(c):04X}\n' for c in chars))
for size in (16, 20):
    subprocess.run([a.node, str(a.converter), '--font', str(asset), '--symbols', chars,
                    '--size', str(size), '--bpp', '4', '--format', 'lvgl', '--no-compress',
                    '--lv-font-name', f'passport_font_{size}', '--lv-include', 'lvgl.h',
                    '--output', str(ROOT / f'assets/fonts/passport_font_{size}.c')], check=True)
    # Converter comments contain the absolute command line; keep assets portable.
    generated = ROOT / f'assets/fonts/passport_font_{size}.c'
    lines = generated.read_text().splitlines()
    generated.write_text('\n'.join(line for line in lines if not line.startswith(' * Opts:')) + '\n')
print(f'Generated 16px and 20px fonts covering {len(chars)} code points.')
