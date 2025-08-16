# NAVIS_Pro
## More than a slide presenter

This document outlines the pin definitions and hardware connections for the **NAVIS_Pro** project using the **ESP32-C3 Super Mini** development board, based on the Espressif ESP32-C3 chip. The board features Wi-Fi, Bluetooth 5.0 LE, and low power consumption in a compact form factor. This README details the assigned pins for buttons, an I2C IMU sensor, a laser pointer, a haptic engine vibrator, and a WS2812 RGB LED, with setup instructions and considerations.

## Hardware Connections

The ESP32-C3 Super Mini has 16 pins, with 11 programmable GPIOs supporting ADC, PWM, UART, I2C, and SPI. Below are the pin assignments for this project, verified against the board’s pinout (refer to [Random Nerd Tutorials - Getting Started with ESP32-C3 Super Mini](https://randomnerdtutorials.com/getting-started-esp32-c3-super-mini/)).

### Pin Definitions

| Component            | GPIO Pin(s) | Function | Notes |
|----------------------|-------------|----------|-------|
| **Buttons**          | GPIO 0, 4, 5, 6, 7 | Digital Input (to GND) | Connected to GND with pull-up resistors (internal or external 10kΩ to 3.3V). Used for button inputs with software debouncing. Avoid SPI conflicts (GPIO 4, 5, 6, 7 are default SPI pins). |
| **I2C Sensor (IMU)** | GPIO 8 (SDA), GPIO 9 (SCL) | I2C Communication | Default I2C pins. Requires external 4.7kΩ pull-up resistors to 3.3V. GPIO 8 is connected to the onboard LED (active LOW); GPIO 9 is connected to the BOOT button (strapping pin). Test for boot issues and consider reassigning to other GPIOs if needed. |
| **Laser Pointer**    | GPIO 3 | Digital Output or PWM | Controls a laser pointer. If the laser requires >3.3V or high current, use a transistor (e.g., NPN or MOSFET) with a 1kΩ resistor to GPIO 3. Supports PWM for brightness control. |
| **Haptic Engine Vibrator** | GPIO 21 | Digital Output or PWM | Controls a vibration motor. Use a transistor or driver (e.g., ULN2003) due to likely high current/voltage requirements. GPIO 21 is the default UART TX pin; reassign UART if needed. Supports PWM for intensity control. |
| **WS2812 RGB LED**  | GPIO 1 | Digital Output (Data) | Controls a WS2812 RGB LED strip or module. Connect the data pin to GPIO 1. Requires a stable 5V power supply and a common ground with the ESP32-C3. Use a library like `Adafruit_NeoPixel` or `FastLED` for control. |

### Pinout Notes
- **Strapping Pins**: GPIO 8 (SDA, onboard LED) and GPIO 9 (SCL, BOOT button) are strapping pins, which may affect boot behavior. Monitor for issues during boot or I2C communication.
- **GPIO 1**: General-purpose I/O, ADC1_CH1, PWM. Suitable for WS2812 RGB LED control, as it supports the precise timing required for WS2812 data signals.
- **Default Interfaces** (Arduino IDE, ESP32C3 Dev Module):
  - **UART**: GPIO 20 (RX), GPIO 21 (TX). Reassign UART (e.g., to GPIO 20 and 10) if using GPIO 21 for the haptic engine.
  - **SPI**: GPIO 4 (SCK), GPIO 5 (SS), GPIO 6 (MISO), GPIO 7 (MOSI). Avoid SPI configuration if using these for buttons.
  - **I2C**: GPIO 8 (SDA), GPIO 9 (SCL). Default for the IMU.
- **Available GPIOs**: GPIO 2, 10, 20 remain unused. Note that GPIO 2 is a strapping pin (avoid for general use), and GPIO 20 is the default UART RX pin.

## Setup Instructions

1. **Buttons**:
   - Connect each button between the respective GPIO (0, 4, 5, 6, 7) and GND.
   - Enable internal pull-ups in software (`pinMode(pin, INPUT_PULLUP)`) or use external 10kΩ resistors to 3.3V.
   - Implement software debouncing (e.g., using the `Bounce2` library or a delay-based approach).
   - Example code:
     ```cpp
     const int buttonPins[] = {0, 4, 5, 6, 7};
     for (int i = 0; i < 5; i++) {
       pinMode(buttonPins[i], INPUT_PULLUP);
     }
     ```

2. **I2C IMU**:
   - Connect the IMU’s SDA to GPIO 8 and SCL to GPIO 9.
   - Add 4.7kΩ pull-up resistors from SDA and SCL to 3.3V.
   - Initialize I2C in Arduino IDE: `Wire.begin(8, 9);`.
   - If the onboard LED on GPIO 8 flickers during I2C communication, disable it: `pinMode(8, OUTPUT); digitalWrite(8, HIGH);` before I2C setup.
   - If boot issues occur, reassign I2C (e.g., `Wire.begin(0, 10);` for GPIO 0 as SDA, GPIO 10 as SCL).
   - Example code:
     ```cpp
     #include <Wire.h>
     void setup() {
       pinMode(8, OUTPUT); digitalWrite(8, HIGH); // Disable LED
       Wire.begin(8, 9); // SDA, SCL
     }
     ```

3. **Laser Pointer**:
   - If 3.3V-compatible and low-current (<20mA), connect directly to GPIO 3 and GND.
   - For 5V or high-current lasers, use a transistor circuit:
     - GPIO 3 → 1kΩ resistor → transistor base/gate.
     - Laser between 5V and transistor collector/drain; emitter/source to GND.
   - Example code:
     ```cpp
     const int laserPin = 3;
     void setup() {
       pinMode(laserPin, OUTPUT);
       digitalWrite(laserPin, LOW); // Off
     }
     void loop() {
       digitalWrite(laserPin, HIGH); // On
       delay(1000);
       digitalWrite(laserPin, LOW); // Off
       delay(1000);
     }
     ```

4. **Haptic Engine Vibrator**:
   - Use a transistor or driver (e.g., ULN2003) to control the haptic engine, as it likely requires >3.3V or high current.
   - Connect GPIO 21 to the transistor base/gate via a 1kΩ resistor; haptic engine between 5V and transistor collector/drain; emitter/source to GND.
   - If using UART, reassign it (e.g., `Serial.begin(115200, SERIAL_8N1, 20, 10);` for RX on GPIO 20, TX on GPIO 10).
   - Example code:
     ```cpp
     const int hapticPin = 21;
     void setup() {
       pinMode(hapticPin, OUTPUT);
       digitalWrite(hapticPin, LOW); // Off
     }
     void loop() {
       digitalWrite(hapticPin, HIGH); // On
       delay(500);
       digitalWrite(hapticPin, LOW); // Off
       delay(500);
     }
     ```

5. **WS2812 RGB LED**:
   - Connect the WS2812 RGB LED’s data pin to GPIO 1, and ensure a common GND with the ESP32-C3.
   - Power the WS2812 with 5V (most WS2812 LEDs require 5V; check datasheet). Use a stable 5V power source capable of supplying sufficient current (e.g., 60mA per LED at full brightness).
   - Optionally, add a 300–500Ω resistor between GPIO 1 and the WS2812 data pin to reduce noise, and a 1000µF capacitor across the 5V and GND lines for power stability.
   - Use a library like `Adafruit_NeoPixel` or `FastLED` for control.
   - Example code (using `Adafruit_NeoPixel`):
     ```cpp
     #include <Adafruit_NeoPixel.h>
     #define LED_PIN 1
     #define NUM_LEDS 1 // Adjust for number of LEDs
     Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
     void setup() {
       strip.begin();
       strip.setBrightness(50); // 0–255
       strip.clear();
     }
     void loop() {
       strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
       strip.show();
       delay(1000);
       strip.setPixelColor(0, strip.Color(0, 255, 0)); // Green
       strip.show();
       delay(1000);
       strip.setPixelColor(0, strip.Color(0, 0, 255)); // Blue
       strip.show();
       delay(1000);
     }
     ```

6. **Power Supply**:
   - Power the board via USB-C (5V) or the 5V pin (3.3V–6V). Do not use both simultaneously.
   - Ensure the power source can supply sufficient current for all components, especially the WS2812 RGB LED (up to 60mA per LED at full brightness) and haptic engine.
   - For deep sleep applications, power off peripherals (e.g., via transistors) and consider removing the power LED to achieve ~43µA (see reference document, comment #22).

7. **Programming**:
   - Use Arduino IDE with the “ESP32C3 Dev Module” or “Nologo ESP32C3 Super Mini” board selection (see reference document, comments #14, #24). Alternatively, try “XIAO_ESP32C3” if upload issues occur (comment #25).
   - Install the ESP32 boards package (version 3.X or 2.0.17, per comment #23) via Boards Manager.
   - For uploads after deep sleep sketches, enter bootloader mode:
     1. Hold BOOT button (GPIO 9).
     2. Press and release RESET button.
     3. Release BOOT button.
   - If upload issues persist, erase the flash: [ESP32 Erase Flash Memory](https://randomnerdtutorials.com/esp32-erase-flash-memory/).

## Considerations
- **Wi-Fi Range**: Some ESP32-C3 Super Mini boards have a design flaw affecting Wi-Fi range due to antenna placement (reference document, comment #4). Test Wi-Fi performance and consider boards with a U.FL connector (comment #5) or an antenna modification (comment #26: [Hackaday Antenna Mod](https://hackaday.com/2025/04/07/simple-antenna-makes-for-better-esp32-c3-wifi/)).
- **I2C Issues**: GPIO 8 and 9 are strapping pins and connected to the onboard LED and BOOT button, respectively, which may cause boot or communication issues (comment #28). Test thoroughly and reassign I2C to GPIO 0 and 10 if needed.
- **Power Consumption**: For deep sleep, ensure peripherals (e.g., WS2812, laser, haptic engine) are powered off via transistors and consider removing the power LED to approach the 43µA specification (comment #22).
- **GPIO Conflicts**:
  - Avoid SPI configuration if using GPIO 4, 5, 6, 7 for buttons.
  - Reassign UART if using GPIO 21 for the haptic engine.
- **WS2812 RGB LED**:
  - Ensure a stable 5V power supply, as voltage drops can cause unreliable LED behavior.
  - Limit the number of LEDs or brightness to manage current draw (e.g., `strip.setBrightness(50);`).
  - Use a level shifter if connecting multiple WS2812 LEDs to ensure reliable data signal (GPIO 1 is 3.3V).

## References
- [Getting Started with the ESP32-C3 Super Mini](https://randomnerdtutorials.com/getting-started-esp32-c3-super-mini/) by Random Nerd Tutorials
- ESP32-C3 Super Mini Datasheet (linked in the reference document)
- [ESP32-C3 Super Mini Antenna Modification](https://hackaday.com/2025/04/07/simple-antenna-makes-for-better-esp32-c3-wifi/) (for Wi-Fi range issues)
- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel) for WS2812 control

## License
This project is licensed under the MIT License, following the example code in the reference document.
