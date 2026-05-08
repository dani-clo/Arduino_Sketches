# Camera Serial Viewer

Python script to receive and display in real-time video frames captured from an Arduino board via serial connection, using i.e. [CameraCaptureRawBytes.ino](https://github.com/arduino/ArduinoCore-zephyr/blob/main/libraries/Camera/examples/CameraCaptureRawBytes/CameraCaptureRawBytes.ino)

## How it works

1. **Board** runs the `CameraCaptureRawBytes.ino` sketch which:
   - Captures frames in RGB565 (320x240 pixels) from the camera
   - Waits for a serial request

2. **PC** runs `camera_serial_viewer.py` which:
   - Sends a frame request byte (0x01) over serial
   - Receives raw RGB565 bytes
   - Converts the format 
   - Displays the live video in an OpenCV window

## Requirements

In a virtual environment
```bash
pip install pyserial numpy opencv-python
```

Or system-wide
```bash
sudo apt install python3-pyserial-asyncio-fast python3-numpy python3-opencv
```

## Usage

Basic command: automatically corrects the format in big-endian for the aforementioned example running with zephyr core on Nicla Vision
```bash
python3 camera_serial_viewer.py --port /dev/ttyACM0 --vflip
```

With orientation options:
```bash
# Flip vertically and mirror horizontally
python3 camera_serial_viewer.py --port /dev/ttyACM0 --vflip --hmirror

Advanced options:
```bash
python3 camera_serial_viewer.py \
  --port /dev/ttyACM0 \
  --baud 115200 \
  --width 320 \
  --height 240 \
  --timeout 2.0 \
  --format rgb565_be
```

## Available arguments

- `--port PORT` (required): Serial port (e.g. `/dev/ttyACM0`)
- `--baud BAUD`: Serial baud rate (default: 115200)
- `--width WIDTH`: Frame width in pixels (default: 320)
- `--height HEIGHT`: Frame height in pixels (default: 240)
- `--timeout TIMEOUT`: Serial read timeout in seconds (default: 1.5)
- `--format FORMAT`: Pixel format (default: `rgb565_be`)
  - `rgb565_be`: RGB565 big-endian
  - `rgb565_le`: RGB565 little-endian
  - `bgr565_be`: BGR565 big-endian
  - `bgr565_le`: BGR565 little-endian
  - `rgb565_byte_swapped`: RGB565 with swapped bytes
- `--vflip`: Flip image vertically
- `--hmirror`: Mirror image horizontally

## Controls

- **Q**: Exit the application

## Troubleshooting

**"Port /dev/ttyACM0 can not be opened"**
- Check the correct port with: `ls /dev/ttyACM*` 
- Make sure the sketch is uploaded to the board and Serial Monitor is **NOT** active.

**Image is upside down or mirrored**
- Use `--vflip` and/or `--hmirror` to correct
- Or modify the sketch by adding:
  ```cpp
  cam.setVerticalFlip(true);
  cam.setHorizontalMirror(true);
  ```
