#!/usr/bin/env python3
"""Live serial viewer for CameraCaptureRawBytes sketch.
Requires packages: pyserial numpy opencv-python.

Protocol expected from the sketch:
- Host sends a single byte 0x01 to request a frame.
- Board replies with raw RGB565 frame bytes only.

Default frame format in the example sketch is 320x240 RGB565.
"""

from __future__ import annotations

import argparse
import sys
import time

import cv2
import numpy as np
import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="View Nicla Vision camera frames over serial")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--width", type=int, default=320, help="Frame width (default: 320)")
    parser.add_argument("--height", type=int, default=240, help="Frame height (default: 240)")
    parser.add_argument("--timeout", type=float, default=1.5, help="Serial read timeout in seconds")
    parser.add_argument(
        "--format",
        default="rgb565_be",
        choices=["rgb565_le", "rgb565_be", "bgr565_le", "bgr565_be", "rgb565_byte_swapped"],
        help="Pixel format and endianness (default: rgb565_be)",
    )
    parser.add_argument(
        "--vflip",
        action="store_true",
        help="Flip image vertically (upside down correction)",
    )
    parser.add_argument(
        "--hmirror",
        action="store_true",
        help="Mirror image horizontally (left-right correction)",
    )
    return parser.parse_args()


def read_exact(ser: serial.Serial, size: int) -> bytes:
    """Read exactly size bytes or raise TimeoutError."""
    data = bytearray()
    deadline = time.monotonic() + max(ser.timeout or 0, 0.01)

    while len(data) < size:
        chunk = ser.read(size - len(data))
        if chunk:
            data.extend(chunk)
            deadline = time.monotonic() + max(ser.timeout or 0, 0.01)
            continue

        if time.monotonic() >= deadline:
            raise TimeoutError(f"Timed out while waiting for frame ({len(data)}/{size} bytes)")

    return bytes(data)


def rgb565_to_bgr(frame_bytes: bytes, width: int, height: int) -> np.ndarray:
    """Convert packed RGB565 bytes to BGR image for OpenCV display."""
    pixels = np.frombuffer(frame_bytes, dtype="<u2").reshape((height, width))

    r = ((pixels >> 11) & 0x1F).astype(np.uint8)
    g = ((pixels >> 5) & 0x3F).astype(np.uint8)
    b = (pixels & 0x1F).astype(np.uint8)

    # Expand to 8-bit channels.
    r = (r << 3) | (r >> 2)
    g = (g << 2) | (g >> 4)
    b = (b << 3) | (b >> 2)

    return np.dstack((b, g, r))


def convert_frame(
    frame_bytes: bytes, width: int, height: int, pixel_format: str
) -> np.ndarray:
    """Convert frame bytes to BGR image based on specified pixel format."""
    if pixel_format == "rgb565_le":
        # RGB565 little-endian (default)
        pixels = np.frombuffer(frame_bytes, dtype="<u2").reshape((height, width))
    elif pixel_format == "rgb565_be":
        # RGB565 big-endian
        pixels = np.frombuffer(frame_bytes, dtype=">u2").reshape((height, width))
    elif pixel_format == "bgr565_le":
        # BGR565 little-endian (channels swapped)
        pixels = np.frombuffer(frame_bytes, dtype="<u2").reshape((height, width))
    elif pixel_format == "bgr565_be":
        # BGR565 big-endian
        pixels = np.frombuffer(frame_bytes, dtype=">u2").reshape((height, width))
    elif pixel_format == "rgb565_byte_swapped":
        # RGB565 with bytes swapped in each uint16
        raw = np.frombuffer(frame_bytes, dtype="<u2")
        pixels = ((raw & 0xFF) << 8) | ((raw >> 8) & 0xFF)
        pixels = pixels.reshape((height, width))
    else:
        raise ValueError(f"Unknown format: {pixel_format}")

    if pixel_format.startswith("bgr"):
        # BGR565 layout: B(5) G(6) R(5)
        b_val = ((pixels >> 11) & 0x1F).astype(np.uint8)
        g_val = ((pixels >> 5) & 0x3F).astype(np.uint8)
        r_val = (pixels & 0x1F).astype(np.uint8)
    else:
        # RGB565 layout: R(5) G(6) B(5)
        r_val = ((pixels >> 11) & 0x1F).astype(np.uint8)
        g_val = ((pixels >> 5) & 0x3F).astype(np.uint8)
        b_val = (pixels & 0x1F).astype(np.uint8)

    # Expand 5-bit and 6-bit values to 8-bit
    r_val = (r_val << 3) | (r_val >> 2)
    g_val = (g_val << 2) | (g_val >> 4)
    b_val = (b_val << 3) | (b_val >> 2)

    return np.dstack((b_val, g_val, r_val))


def main() -> int:
    args = parse_args()
    frame_size = args.width * args.height * 2

    try:
        with serial.Serial(args.port, args.baud, timeout=args.timeout) as ser:
            # Let the USB CDC/serial line settle.
            time.sleep(0.3)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            print(
                f"Connected to {args.port} at {args.baud} bps | "
                f"{args.width}x{args.height} {args.format}"
            )
            if args.vflip or args.hmirror:
                flip_str = []
                if args.vflip:
                    flip_str.append("vflip")
                if args.hmirror:
                    flip_str.append("hmirror")
                print(f"Transforms: {', '.join(flip_str)}")
            print("Press q in the image window to quit")

            while True:
                ser.write(b"\x01")
                frame_bytes = read_exact(ser, frame_size)
                frame = convert_frame(frame_bytes, args.width, args.height, args.format)

                if args.vflip:
                    frame = cv2.flip(frame, 0)  # 0 = flip vertically
                if args.hmirror:
                    frame = cv2.flip(frame, 1)  # 1 = flip horizontally

                cv2.imshow("Nicla Vision Serial Camera", frame)
                key = cv2.waitKey(1) & 0xFF
                if key == ord("q"):
                    break

    except KeyboardInterrupt:
        pass
    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        return 1
    except TimeoutError as exc:
        print(f"Frame timeout: {exc}", file=sys.stderr)
        return 2
    finally:
        cv2.destroyAllWindows()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
