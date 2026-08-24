#!/usr/bin/env python3
"""SD Card batch converter for STM32 TFT.
Converts: .jpg/.jpeg -> .raw (240x240 RGB565 image)
          .mp4/.avi/.mov/.gif/.webm -> .vid (240x240 RGB565 video)
Usage: py -3 convert_all.py E:/
"""
import os, sys, struct
from PIL import Image

SD = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
print(f"SD Card: {SD}\n")

# ---- Image conversion ----
def convert_image(jpg_path):
    raw_path = os.path.splitext(jpg_path)[0] + ".raw"
    img = Image.open(jpg_path).convert("RGB")
    w, h = img.size
    scale = min(240 / w, 240 / h)
    nw, nh = int(w * scale), int(h * scale)
    img = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGB", (240, 240), (0, 0, 0))
    ox, oy = (240 - nw) // 2, (240 - nh) // 2
    canvas.paste(img, (ox, oy))
    raw = bytearray(240 * 240 * 2)
    pixels = canvas.load()
    for y in range(240):
        for x in range(240):
            r, g, b = pixels[x, y]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            off = (y * 240 + x) * 2
            raw[off] = rgb565 & 0xFF
            raw[off + 1] = (rgb565 >> 8) & 0xFF
    with open(raw_path, "wb") as f:
        f.write(raw)
    return nw, nh

# ---- Video conversion ----
def convert_video(vid_path):
    import cv2, numpy as np
    out_path = os.path.splitext(vid_path)[0] + ".vid"
    fps = 20
    max_frames = 800

    cap = cv2.VideoCapture(vid_path)
    src_fps = cap.get(cv2.CAP_PROP_FPS)
    if src_fps <= 0: src_fps = 30
    step = max(1, int(src_fps / fps))

    frames = []
    idx = 0
    while len(frames) < max_frames:
        ret, frame = cap.read()
        if not ret: break
        if idx % step == 0:
            h, w = frame.shape[:2]
            s = min(240 / w, 240 / h)
            nw, nh = int(w * s), int(h * s)
            resized = cv2.resize(frame, (nw, nh), interpolation=cv2.INTER_LANCZOS4)
            canvas = np.zeros((240, 240, 3), dtype=np.uint8)
            ox, oy = (240 - nw) // 2, (240 - nh) // 2
            canvas[oy:oy+nh, ox:ox+nw] = resized
            r, g, b = canvas[:,:,2], canvas[:,:,1], canvas[:,:,0]
            rgb565 = ((r.astype(np.uint16) & 0xF8) << 8) | \
                     ((g.astype(np.uint16) & 0xFC) << 3) | \
                     (b.astype(np.uint16) >> 3)
            flat = rgb565.flatten()
            total = len(flat)
            raw = bytearray(total * 2)
            for j in range(total):
                raw[j*2]   = flat[j] & 0xFF
                raw[j*2+1] = (flat[j] >> 8) & 0xFF
            frames.append(bytes(raw))
        idx += 1
    cap.release()

    if frames:
        with open(out_path, 'wb') as f:
            f.write(struct.pack('<I', len(frames)))        # frame count
            f.write(struct.pack('<I', fps * 100))          # fps * 100
            for fr in frames: f.write(fr)
        size_mb = os.path.getsize(out_path) / (1024 * 1024)
        return len(frames), size_mb
    return 0, 0

# ---- Process all files ----
img_count = 0
vid_count = 0

for fn in sorted(os.listdir(SD)):
    low = fn.lower()
    if low.endswith(('.jpg', '.jpeg')):
        nw, nh = convert_image(os.path.join(SD, fn))
        print(f"  [IMG] {fn} -> {os.path.splitext(fn)[0]}.raw  ({nw}x{nh})")
        img_count += 1

for fn in sorted(os.listdir(SD)):
    low = fn.lower()
    if low.endswith(('.mp4', '.avi', '.mov', '.gif', '.webm')):
        print(f"  [VID] Converting {fn}...")
        n, mb = convert_video(os.path.join(SD, fn))
        if n > 0:
            print(f"         -> {os.path.splitext(fn)[0]}.vid  ({n} frames, {mb:.1f} MB)")
            vid_count += 1

print(f"\nDone! {img_count} image(s), {vid_count} video(s) converted.")
input("Press Enter to close...")
