# UnoQArcade

Arcade mini-pack for Arduino Uno Q using the built-in 13x8 LED Matrix and one analog joystick.

The sketch includes:
- Snake
- Tetris
- Breakout

At startup you choose difficulty from Serial, then select the game from a matrix menu.

## Features

- Single sketch with 3 games
- Built-in LED Matrix support (no external display needed)
- Joystick auto-calibration at boot
- Menu on matrix with scrolling game name
- Unified difficulty level (0 to 9)
- Long-press SW in-game to return to menu

## Hardware Requirements

- 1x Arduino Uno Q
- 1x analog joystick module (VRx, VRy, SW)
- Jumper wires

## Wiring

Connect joystick pins as follows:

| Joystick pin | Arduino Uno Q pin |
| --- | --- |
| VRx | A0 |
| VRy | A1 |
| SW | D7 |
| VCC | 5V |
| GND | GND |

## Software Requirements

- Arduino core/platform with Uno Q support 
- Libraries:
	- ArduinoGraphics
	- Arduino_LED_Matrix

## Startup Flow

1. Power on / reset
2. Keep joystick centered during auto-calibration
3. Enter difficulty on Serial (`0 = easiest`, `9 = hardest`)
4. Use menu to choose a game

## Menu Controls

- Move joystick left/right: change selected game
- Press SW: start selected game

Menu display:
- Top rows: selection indicator
- Scrolling game name (Snake, Tetris, Breakout)

## In-Game Universal Controls

- Hold SW for about 1 second: return to menu

## Game Rules and Controls

### Snake

Controls:
- Joystick in 4 directions

Rules:
- Wrap-around on all edges
- Eat food to grow and gain score
- Collision with self ends the run

Output:
- Score updates on Serial

### Tetris

Controls:
- Left/Right: move piece
- Down: soft drop
- SW press: rotate piece

Rules:
- Vertical 8x13 playfield mapped to the 13x8 display
- Clear lines to score
- Game ends when new piece cannot spawn

Output:
- Lines and score on Serial

### Breakout

Controls:
- Joystick X: paddle movement
- SW press: launch ball (when attached to paddle)

Rules:
- Break all bricks to win
- Lose a life when the ball falls below paddle
- Lose when lives reach zero

Output:
- Score/lives on Serial

## Difficulty

Difficulty value `0..9` affects game speed:
- Snake: snake step interval
- Tetris: gravity speed
- Breakout: ball speed

Higher difficulty means faster gameplay.

## Troubleshooting

### Menu or game moves by itself

- Check joystick wiring and GND
- Reboot and keep joystick centered during calibration

### Compilation errors about missing headers

- Install/check `ArduinoGraphics` and `Arduino_LED_Matrix`
- Verify board core and include paths are correctly configured


