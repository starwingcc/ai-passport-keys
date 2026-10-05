<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Passport Keys fonts

The application embeds four read-only LVGL fonts: dynamic 16/20/28 px
at 2 bpp with lossless RLE compression and uncompressed fixed-interface 20 px
at 4 bpp. `passport-codepoints.txt` lists 3,886
dynamic glyphs; `passport-ui-codepoints.txt` lists 229 fixed glyphs. Labels explicitly
select their font; all menu rows use `passport_font_20_menu`, including dynamic
scenario names. Start control retains its 16 px mappings and fixed 20 px key labels.
The compressed 16/28 px fonts preserve their original pixels and metrics.
All subsets fit the compact 20-bit bitmap offsets;
`CONFIG_LV_FONT_FMT_TXT_LARGE=n` halves the glyph-descriptor storage.
`CONFIG_LV_USE_FONT_COMPRESSED=y` enables the bounded glyph decoder.

Source: [Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf),
SHA-256 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.
The SIL Open Font License is retained in [fonts/OFL.txt](fonts/OFL.txt).
The subset is renamed `PassportKeysSubset`; [fonts/passport-keys-subset.otf](fonts/passport-keys-subset.otf)
retains the actual outlines. No runtime font-file loading is needed.

The inventory includes the 3,755 GB2312 level-one Hanzi, printable ASCII,
common Chinese punctuation, ellipsis, direction arrows and every fixed-interface
character. The reproducible selection is in `tools/keypad_font_inventory.py`.
New unsupported characters, including uncommon Hanzi and emoji, are rejected by
the shared C/JavaScript coverage tables. Existing v2/v3/v4 names and shortcuts survive
an inventory change: stored UTF-8 text remains intact, missing glyphs show visible
placeholders on the device, and the browser shows the original names for editing.
Unsupported names must be corrected before saving from the browser. This inventory
does not cover all Chinese characters or Unicode.

Generate with FontTools 4.61.1, lv_font_conv 1.5.3 and Node.js:

```sh
python3 tools/generate_keypad_fonts.py \
  --source /path/to/NotoSansCJKsc-Regular.otf \
  --converter /path/to/lv_font_conv/lv_font_conv.js --node /path/to/node
python3 tests/test_keypad_fonts.py
```

The generator checks the source cmap, includes fixed text from `main/main.c` and
`main/keyboard/*.c` except web-only code, updates inventories and C/web coverage,
and converts without kerning, using compression for the three dynamic sizes.
Regenerate after changing device text. Static tests parse all four cmaps,
including sparse/dense missing entries;
LVGL previews verify actual glyph lookup, a missing-glyph negative case and screen
layout. Check Flash size and runtime heap/stack separately; real-screen acceptance
remains necessary. See the [application guide](../docs/assets/passport-keys.md).

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
