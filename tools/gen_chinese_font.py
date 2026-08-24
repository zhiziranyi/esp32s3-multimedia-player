#!/usr/bin/env python3
"""Generate a custom 16x16 Chinese font C array from a text file.
Extracts all unique CJK chars, renders with SimSun font, outputs C code.
Usage: python gen_chinese_font.py input.txt [output.c]
"""
import sys, os, struct
from PIL import Image, ImageFont, ImageDraw

# SimSun font path on Windows
FONT_PATH = "C:/Windows/Fonts/simsun.ttc"
FONT_SIZE = 16

def extract_chars(text):
    """Return sorted unique non-ASCII chars from UTF-8 text."""
    chars = set()
    for ch in text:
        if ord(ch) > 127:  # non-ASCII
            chars.add(ch)
    # Add ASCII chars as fallback (not needed for font, just for rendering check)
    return sorted(chars, key=ord)

def render_char(ch, font):
    """Render one character to 16x16 1bpp column-major bitmap (32 bytes).
    Format matches PCtoLCD2002 C51 column-major (like reference code)."""
    img = Image.new("1", (16, 16), 0)
    draw = ImageDraw.Draw(img)
    # Center the glyph (some fonts have different metrics)
    draw.text((0, -1), ch, font=font, fill=1)

    # Convert to column-major, LSB=top pixel (PCtoLCD2002 C51 format)
    bitmap = bytearray(32)
    for col in range(16):
        for byte_ofs in range(2):  # 2 bytes per column for 16px height
            val = 0
            for bit in range(8):
                y = byte_ofs * 8 + bit
                if y < 16 and img.getpixel((col, y)):
                    val |= (1 << bit)
            bitmap[col * 2 + byte_ofs] = val
    return bytes(bitmap)

def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} input.txt [output.c]")
        sys.exit(1)

    txt_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else "cn_font.c"

    # Read text and extract unique Chinese chars
    with open(txt_path, "rb") as f:
        raw = f.read()
        # Skip UTF-8 BOM if present
        if raw[:3] == b'\xef\xbb\xbf':
            raw = raw[3:]
    text = raw.decode("utf-8", errors="replace")
    chars = extract_chars(text)
    print(f"Found {len(chars)} unique non-ASCII chars")

    # Load font
    font = ImageFont.truetype(FONT_PATH, FONT_SIZE, index=0)

    # Generate C code
    lines = []
    lines.append("// Auto-generated Chinese font for LVGL/TFT")
    lines.append(f"// Chars: {len(chars)}, Font: SimSun {FONT_SIZE}px")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define CN_FONT_COUNT {len(chars)}")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    uint16_t code;   // Unicode code point")
    lines.append("    uint8_t  bmp[32]; // 16x16 col-major, 1bpp, LSB=top")
    lines.append("} cn_glyph_t;")
    lines.append("")
    lines.append("static const cn_glyph_t cn_font[] = {")

    for ch in chars:
        code = ord(ch)
        bmp = render_char(ch, font)
        hex_bytes = ", ".join(f"0x{b:02X}" for b in bmp)
        lines.append(f"    {{0x{code:04X}, {{{hex_bytes}}}}},  // U+{code:04X} {ch}")

    lines.append("};")
    lines.append("")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"Wrote {out_path} ({len(chars)} glyphs)")

if __name__ == "__main__":
    main()
