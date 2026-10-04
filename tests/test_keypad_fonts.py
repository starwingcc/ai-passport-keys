#!/usr/bin/env python3
"""Verify generated cmaps, web/device input coverage and actual font bindings."""
from pathlib import Path
import json
import re
root=Path(__file__).resolve().parent.parent
inventory={int(x[2:],16) for x in (root/'assets/fonts/passport-codepoints.txt').read_text().splitlines()}
fixed={int(x[2:],16) for x in (root/'assets/fonts/passport-ui-codepoints.txt').read_text().splitlines()}
text=(root/'main/main.c').read_text()
for path in (root/'main/keyboard').glob('*.c'):
    if path.name!='keypad_web.c':text+=path.read_text()
required=set(range(32,127))|{ord(c) for c in text if ord(c)>=128}
assert required<=fixed<=inventory
for size in (16,20,28):
    source=(root/f'assets/fonts/passport_font_{size}.c').read_text()
    covered=set()
    for block in re.search(r'cmaps\[\].*?=\s*\{(.*?)\n\};',source,re.S)[1].split('},'):
        start=int(re.search(r'range_start = (\d+)',block)[1]);length=int(re.search(r'range_length = (\d+)',block)[1])
        assert int(re.search(r'glyph_id_start = (\d+)',block)[1])>0
        name=re.search(r'unicode_list = (\w+)',block)[1]
        if name=='NULL':
            offsets_name=re.search(r'glyph_id_ofs_list = (\w+)',block)[1]
            if offsets_name=='NULL':covered.update(range(start,start+length))
            else:
                data=re.search(offsets_name+r'\[\] = \{(.*?)\};',source,re.S)[1]
                ids=[int(x,0) for x in re.findall(r'0x[0-9a-f]+|\d+',data)]
                covered.update(start+i for i,value in enumerate(ids) if i==0 or value)
        else:
            data=re.search(name+r'\[\] = \{(.*?)\};',source,re.S)[1]
            offsets=[int(x,0) for x in re.findall(r'0x[0-9a-f]+|\d+',data)]
            assert all(0<=v<length for v in offsets)
            covered.update(start+v for v in offsets)
    assert covered==(fixed if size==20 else inventory),(size,inventory-covered)
    assert 0x1f600 not in covered
    assert '.bitmap_format = 0' in source
    assert f'.bpp = {4 if size==20 else 2}' in source
c_ranges=[(int(a,16),int(b,16)) for a,b in re.findall(r'\{0x([0-9a-f]+), 0x([0-9a-f]+)\}',(root/'main/keyboard/keypad_charset.h').read_text())]
web=json.loads(re.search(r'= (\[.*\]);',(root/'main/web/charset.js').read_text())[1])
assert web==[list(r) for r in c_ranges]
assert {cp for a,b in c_ranges for cp in range(a,b+1)}==inventory
ui=(root/'main/keyboard/keypad_ui.c').read_text()
assert all('&passport_font_'+str(size) in ui for size in (16,20,28))
assert 'command ? &passport_font_16 : &passport_font_20' in ui
assert 'ui_pixel' not in ui
print(f'Keypad fonts: PASS ({len(inventory)} dynamic glyphs at 16/28px, {len(fixed)} fixed glyphs at 20px)')
