#include <Bounce2.h>

#define NUM_BUTTONS 5
uint8_t buttonPins[NUM_BUTTONS] = {0, 4, 5, 6, 7};

Bounce debouncers[NUM_BUTTONS];
unsigned long pressStart[NUM_BUTTONS];
uint8_t pressCount[NUM_BUTTONS];

unsigned long multiPressWindow = 400; // ms window for multi-press
unsigned long holdThreshold = 1000;   // ms for long press

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    debouncers[i].attach(buttonPins[i]);
    debouncers[i].interval(25); // debounce time
    pressStart[i] = 0;
    pressCount[i] = 0;
  }

  Serial.println("QC Button Test Ready!");
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    debouncers[i].update();

    if (debouncers[i].fell()) {  
      // Button pressed
      pressStart[i] = millis();
      pressCount[i]++;
    }

    if (debouncers[i].rose()) {  
      // Button released
      unsigned long pressTime = millis() - pressStart[i];

      if (pressTime >= holdThreshold) {
        Serial.printf("GPIO %d -> LONG PRESS (%lu ms)\n", buttonPins[i], pressTime);
        pressCount[i] = 0; // reset after hold
      } else {
        // check for multi-press within window
        delay(10); // tiny debounce assist
        if (millis() - pressStart[i] < multiPressWindow) {
          // still waiting for possible multi press
        }
      }
    }

    // Multi-press detection timeout
    if (pressCount[i] > 0 && (millis() - pressStart[i]) > multiPressWindow) {
      if (pressCount[i] == 1) {
        Serial.printf("GPIO %d -> SINGLE PRESS\n", buttonPins[i]);
      } else {
        Serial.printf("GPIO %d -> %d MULTI-PRESS\n", buttonPins[i], pressCount[i]);
      }
      pressCount[i] = 0;
    }
  }
}
