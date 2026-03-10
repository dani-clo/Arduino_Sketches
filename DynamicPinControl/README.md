# DynamicPinControl

`DynamicPinControl` is a test sketch for **Arduino GIGA** (GIGA R1 WiFi) to validate dynamic pin reconfiguration at runtime.

It exercises transitions between peripheral and GPIO modes on selected pins, then checks that peripherals can be re-acquired correctly.

## What It Tests

The sketch runs a single test cycle (then stops) with these checks:

1. **UART2 -> GPIO on D18**
- Starts `Serial1` on UART2
- Sends one test line
- Ends UART
- Reconfigures D18 as GPIO and toggles it

2. **ADC -> GPIO -> ADC on A0/A3 (manual intervention)**
- Reads A0 and A3 as analog inputs
- Waits for manual rewiring to a logic analyzer
- Reconfigures A0/A3 as GPIO and toggles them
- Waits for manual reconnection
- Reads A0/A3 again and prints PASS/FAIL based on expected values

3. **SPI -> GPIO -> SPI re-acquire on D11 path**
- Performs SPI1 transfer on D11 path
- Switches D11 to GPIO and toggles it
- Re-enables SPI1 and performs another transfer

4. **SPI -> PWM -> GPIO on D11**
- Performs a short SPI1 transfer
- Switches D11 to PWM (`analogWrite`) with two duty cycles
- Disables PWM and finally drives D11 as static GPIO
- Useful to verify that the same pin can move across SPI, timer/PWM, and GPIO roles

## Pin Mapping Used by the Sketch

- `D18`: UART2 TX pin (`Serial1`), then GPIO test
- `D11`: SPI1 activity path, then GPIO, then SPI1 again
- `A0`, `A3`: analog test pins switched to GPIO and back to ADC

## Hardware Setup

The sketch expects this setup:

- Serial analyzer/ Logic analyzer / scope channel on **D18** (UART -> GPIO transition)
- Logic analyzer / scope channel on **D11** (SPI/GPIO transitions)
- For ADC checks:
- Connect **A0 -> GND**
- Connect **A3 -> 3.3V**

During the ADC test, serial output asks you to:

- Disconnect A0/A3 from analog sources and connect to logic analyzer (10 s window)
- Reconnect A0/A3 back to GND/3.3V (10 s window)




