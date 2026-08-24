#!/usr/bin/env python3
"""Convert JPEG images to 240x240 RGB565 raw binary for STM32 TFT display.
Usage: python convert_jpeg.py input.jpg [output.raw]
The .raw file can be put on the SD card and displayed directly.
"""
import sys, struct, os
from PIL import Image

def convert(in_path, out_path=None):
    if out_path is None:
        out_path = os.path.splitext(in_path)[0] + ".raw"

    img = Image.open(in_path).convert("RGB")
    w, h = img.size
    print(f"Source: {w}x{h}")

    # Scale to fit 240x240, preserving aspect ratio, pad with black
    scale = min(240 / w, 240 / h)
    nw, nh = int(w * scale), int(h * scale)
    img = img.resize((nw, nh), Image.LANCZOS)
    print(f"Scaled: {nw}x{nh}")

    # Create 240x240 black canvas and paste centered
    canvas = Image.new("RGB", (240, 240), (0, 0, 0))
    ox, oy = (240 - nw) // 2, (240 - nh) // 2
    canvas.paste(img, (ox, oy))

    # Convert to RGB565
    raw = bytearray(240 * 240 * 2)
    pixels = canvas.load()
    for y in range(240):
        for x in range(240):
            r, g, b = pixels[x, y]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            off = (y * 240 + x) * 2
            raw[off] = rgb565 & 0xFF           # little-endian low byte first
            raw[off + 1] = (rgb565 >> 8) & 0xFF  # high byte

    with open(out_path, "wb") as f:
        f.write(raw)
    print(f"Wrote {out_path} ({len(raw)} bytes, 240x240 RGB565)")
    return out_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} input.jpg [output.raw]")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
