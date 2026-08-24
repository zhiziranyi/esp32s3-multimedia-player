"""
Screen stream sender for ESP32-S3 MP4.
Captures desktop, resizes to 240x240, sends raw RGB565 over TCP.

Usage: python screen_sender.py <ESP32_IP> [fps]
  e.g.  python screen_sender.py 192.168.43.150 10

Requirements: pip install pillow
"""
import socket
import sys
import time
from PIL import Image, ImageGrab

PORT = 8888
W, H = 240, 240

sock = None


def connect(host):
    global sock
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.settimeout(3)
        s.connect((host, PORT))
        sock = s
        print(f"Connected to {host}:{PORT}")
        return True
    except Exception as e:
        print(f"Connect failed: {e}, retrying...")
        sock = None
        return False


def send_frame():
    global sock
    if sock is None:
        return
    try:
        img = ImageGrab.grab()
        img = img.resize((W, H), Image.LANCZOS).convert("RGB")
        pixels = img.load()
        buf = bytearray(W * H * 2)
        idx = 0
        for y in range(H):
            for x in range(W):
                r, g, b = pixels[x, y]
                val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                buf[idx] = val & 0xFF
                buf[idx + 1] = (val >> 8) & 0xFF
                idx += 2
        sock.sendall(bytes(buf))
    except Exception:
        sock = None


def main(host, fps):
    print(f"Streaming {fps}fps -> {host}:{PORT}")
    interval = 1.0 / fps

    while True:
        if sock is None:
            connect(host)
            time.sleep(1)
            continue

        t0 = time.time()
        send_frame()
        elapsed = time.time() - t0
        if elapsed < interval:
            time.sleep(interval - elapsed)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <ESP32_IP> [fps]")
        sys.exit(1)
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 10)
