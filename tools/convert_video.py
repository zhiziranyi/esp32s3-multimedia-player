#!/usr/bin/env python3
"""Convert video/GIF to .vid format for STM32 TFT player.
Usage: py -3 convert_video.py input.mp4 [output.vid] [--fps 8] [--max-frames 200]

.vid format: 4-byte LE frame count, then 240x240 RGB565 frames.
Requires: pip install pillow opencv-python
"""
import sys, os, struct
import numpy as np

try:
    import cv2
except ImportError:
    print("Please install opencv-python: pip install opencv-python")
    sys.exit(1)

def rgb_to_565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def frame_to_raw(frame):
    """Resize frame to 240x240, letterbox, convert to RGB565 bytes."""
    h, w = frame.shape[:2]
    scale = min(240 / w, 240 / h)
    nw, nh = int(w * scale), int(h * scale)
    resized = cv2.resize(frame, (nw, nh), interpolation=cv2.INTER_LANCZOS4)

    # Letterbox on black canvas
    canvas = np.zeros((240, 240, 3), dtype=np.uint8)
    ox, oy = (240 - nw) // 2, (240 - nh) // 2
    canvas[oy:oy+nh, ox:ox+nw] = resized

    # Convert to RGB565 little-endian
    r = canvas[:,:,2]
    g = canvas[:,:,1]
    b = canvas[:,:,0]
    rgb565 = ((r.astype(np.uint16) & 0xF8) << 8) | \
             ((g.astype(np.uint16) & 0xFC) << 3) | \
             (b.astype(np.uint16) >> 3)
    # Convert to little-endian byte array (STM32 is LE)
    raw = bytearray(len(rgb565) * 2)
    flat = rgb565.flatten()
    for i in range(len(flat)):
        raw[i*2]   = flat[i] & 0xFF          # low byte first
        raw[i*2+1] = (flat[i] >> 8) & 0xFF   # high byte second
    return bytes(raw)

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    fps = 8
    max_frames = 500
    for a in sys.argv[1:]:
        if a.startswith('--fps='): fps = int(a.split('=')[1])
        if a.startswith('--max-frames='): max_frames = int(a.split('=')[1])

    if len(args) < 1:
        print("Usage: py -3 convert_video.py input.mp4 [output.vid] [--fps=8] [--max-frames=200]")
        sys.exit(1)

    in_path = args[0]
    out_path = args[1] if len(args) > 1 else os.path.splitext(in_path)[0] + ".vid"

    cap = cv2.VideoCapture(in_path)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    src_fps = cap.get(cv2.CAP_PROP_FPS)
    if src_fps <= 0: src_fps = 30
    step = max(1, int(src_fps / fps))

    print(f"Source: {total_frames} frames @ {src_fps:.1f} fps")
    print(f"Output: every {step} frame(s) @ {fps} fps, max {max_frames} frames")

    frames = []
    idx = 0
    while len(frames) < max_frames:
        ret, frame = cap.read()
        if not ret: break
        if idx % step == 0:
            frames.append(frame_to_raw(frame))
            print(f"  Frame {len(frames)}/{max_frames}", end='\r')
        idx += 1
    cap.release()

    if not frames:
        print("No frames extracted!")
        sys.exit(1)

    with open(out_path, 'wb') as f:
        f.write(struct.pack('<I', len(frames)))  # frame count LE
        for raw in frames:
            f.write(raw)

    size_mb = os.path.getsize(out_path) / (1024*1024)
    print(f"\nWrote {out_path}: {len(frames)} frames, {size_mb:.1f} MB")

if __name__ == "__main__":
    main()
