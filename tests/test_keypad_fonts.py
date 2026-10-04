#!/usr/bin/env python3
"""Check generated LVGL cmap coverage without requiring ESP-IDF or fontTools."""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
inventory={int(x[2:],16) for x in (root/'assets/fonts/passport-codepoints.txt').read_text().splitlines()}
text=(root/'main/main.c').read_text()
for path in (root/'main/keyboard').glob('*.c'):
    if path.name != 'keypad_web.c': text+=path.read_text()
required=set(range(32,127))|{ord(c) for c in text if ord(c)>=128}
assert required <= inventory, f'New UI text missing from inventory: {required-inventory}'
for size in (16,20):
    source=(root/f'assets/fonts/passport_font_{size}.c').read_text()
    covered=set()
    for block in re.search(r'cmaps\[\].*?=\s*\{(.*?)\n\};',source,re.S)[1].split('},'):
        start=int(re.search(r'range_start = (\d+)',block)[1])
        length=int(re.search(r'range_length = (\d+)',block)[1])
        glyph=int(re.search(r'glyph_id_start = (\d+)',block)[1]);assert glyph>0
        name=re.search(r'unicode_list = (\w+)',block)[1]
        if name=='NULL': covered.update(range(start,start+length))
        else:
            data=re.search(name+r'\[\] = \{(.*?)\};',source,re.S)[1]
            offsets=[int(x,0) for x in re.findall(r'0x[0-9a-f]+|\d+',data)]
            assert all(0<=v<length for v in offsets)
            covered.update(start+v for v in offsets)
    assert inventory==covered, (size,inventory-covered,covered-inventory)
    assert 0x9f98 not in covered, 'Known missing negative case unexpectedly covered'
    assert '.bitmap_format = 0' in source
ui=(root/'main/keyboard/keypad_ui.c').read_text()
assert '&passport_font_16' in ui and '&passport_font_20' in ui
assert 'ui_pixel' not in ui
print(f'Keypad font cmaps: PASS ({len(inventory)} glyphs, 16/20px; negative case U+9F98)')
