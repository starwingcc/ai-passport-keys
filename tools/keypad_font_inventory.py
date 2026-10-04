"""Common display inventory, independent of the font conversion toolchain."""


def common_codepoints():
    # GB2312 level-one Hanzi occupy rows 16 through 55 (3,755 characters).
    # Decode the standard codec rather than selecting a Unicode prefix, which
    # would retain uncommon characters while omitting everyday words.
    characters = set(range(32, 127))
    for row in range(0xb0, 0xd8):
        for column in range(0xa1, 0xff):
            try:
                characters.add(ord(bytes((row, column)).decode('gb2312')))
            except UnicodeDecodeError:
                pass  # The final level-one row has unassigned cells.
    characters.update({
        0x3000, 0x3001, 0x3002, 0x3008, 0x3009, 0x300a, 0x300b,
        0x300c, 0x300d, 0x300e, 0x300f, 0x3010, 0x3011, 0x3014, 0x3015,
        0xff01, 0xff08, 0xff09, 0xff0c, 0xff0e, 0xff1a, 0xff1b, 0xff1f, 0xff5e,
        0x2014, 0x2018, 0x2019, 0x201c, 0x201d, 0x2026,
        0x2190, 0x2191, 0x2192, 0x2193,
    })
    return characters
