#!/usr/bin/env python3
"""Verify generated cmaps, web/device input coverage and actual font bindings."""
from pathlib import Path
import json
import re
import sys
root=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(root/'tools'))
from keypad_font_inventory import common_codepoints
inventory={int(x[2:],16) for x in (root/'assets/fonts/passport-codepoints.txt').read_text().splitlines()}
fixed={int(x[2:],16) for x in (root/'assets/fonts/passport-ui-codepoints.txt').read_text().splitlines()}
text=(root/'main/main.c').read_text()
for path in (root/'main/keyboard').glob('*.c'):
    if path.name!='keypad_web.c':text+=path.read_text()
required=set(range(32,127))|{ord(c) for c in text if ord(c)>=128}
assert required<=fixed<=inventory
assert inventory==common_codepoints()|fixed
assert 3800<=len(inventory)<4000
assert {ord(c) for c in '编程浏览，。！？“”《》ABC 123…←→'}<=inventory
assert all(ord(c) not in inventory for c in '龘靐齉')
for name,bpp,compressed,expected in [
        ('passport_font_16',2,True,inventory),
        ('passport_font_20',4,False,fixed),
        ('passport_font_20_menu',2,True,inventory),
        ('passport_font_28',2,True,inventory)]:
    source=(root/f'assets/fonts/{name}.c').read_text()
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
    assert covered==expected,(name,expected-covered)
    assert 0x1f600 not in covered
    assert f'.bitmap_format = {int(compressed)}' in source
    assert f'.bpp = {bpp}' in source
    # Compact descriptors must represent every bitmap offset and glyph metric.
    assert max(map(int,re.findall(r'\.bitmap_index = (\d+)',source)))<1<<20
    assert max(map(int,re.findall(r'\.adv_w = (\d+)',source)))<1<<12
    for metric in ('box_w','box_h'):
        assert max(map(int,re.findall(r'\.'+metric+r' = (\d+)',source)))<1<<8
    for metric in ('ofs_x','ofs_y'):
        values=list(map(int,re.findall(r'\.'+metric+r' = (-?\d+)',source)))
        assert min(values)>=-128 and max(values)<=127
c_ranges=[(int(a,16),int(b,16)) for a,b in re.findall(r'\{0x([0-9a-f]+), 0x([0-9a-f]+)\}',(root/'main/keyboard/keypad_charset.h').read_text())]
web=json.loads(re.search(r'= (\[.*\]);',(root/'main/web/charset.js').read_text())[1])
assert web==[list(r) for r in c_ranges]
assert {cp for a,b in c_ranges for cp in range(a,b+1)}==inventory
ui=(root/'main/keyboard/keypad_ui.c').read_text()
assert all('&passport_font_'+str(size) in ui for size in (16,20,28))
assert 'command ? &passport_font_16 : &passport_font_20' in ui
assert '&passport_font_20_menu' in ui
assert 'CONFIG_LV_USE_FONT_COMPRESSED=y' in (root/'sdkconfig.defaults').read_text()
assert 'CONFIG_LV_FONT_FMT_TXT_LARGE=n' in (root/'sdkconfig.defaults').read_text()
assert '#define LV_FONT_FMT_TXT_LARGE 0' in (root/'tests/keypad_preview/lv_conf.h').read_text()
assert 'ui_pixel' not in ui
print(f'Keypad fonts: PASS ({len(inventory)} dynamic glyphs at 16/20/28px, {len(fixed)} fixed glyphs at 20px)')
