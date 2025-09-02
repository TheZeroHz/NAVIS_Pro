#include <Bounce2.h>
#include "BleKeyboard.h"

BleKeyboard bleKeyboard;

// Pin definitions
#define BTN_PIN         4   // Main button (ESC/F5 toggle)
#define PAGEUP_BTN_PIN  5   // Page Up button
#define PAGEDOWN_BTN_PIN 6  // Page Down button
#define LED_PIN         2   // Status LED

// Button objects
Bounce mainBtn = Bounce();
Bounce pageUpBtn = Bounce();
Bounce pageDownBtn = Bounce();

// Main button state tracking
unsigned long lastTapTime = 0;
unsigned long holdThreshold = 200;       // ms to detect hold
unsigned long doubleTapThreshold = 400;  // ms between taps
bool holding = false;
int tapCount = 0;
bool waitingForSecondTap = false;
bool escMode = true;  // Toggle between ESC and F5

// Blink control - handled by RTOS task
volatile bool blinkingActive = false;
TaskHandle_t blinkTaskHandle = NULL;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Configure LED pin FIRST and ensure it's OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Configure strapping pins as inputs with pull-up for safe boot
  pinMode(8, INPUT_PULLUP);  // GPIO 8 (Onboard LED, I2C SDA) 
  pinMode(9, INPUT_PULLUP);  // GPIO 9 (BOOT Button, I2C SCL)
  delay(100);
  
  // Initialize button objects
  mainBtn.attach(BTN_PIN, INPUT_PULLUP);
  mainBtn.interval(10);
  
  pageUpBtn.attach(PAGEUP_BTN_PIN, INPUT_PULLUP);
  pageUpBtn.interval(10);
  
  pageDownBtn.attach(PAGEDOWN_BTN_PIN, INPUT_PULLUP);
  pageDownBtn.interval(10);
  
  // Create blink task - runs continuously
  xTaskCreate(
    blinkTask,          // Task function
    "BlinkTask",        // Task name
    1000,               // Stack size
    NULL,               // Parameters
    1,                  // Priority
    &blinkTaskHandle    // Task handle
  );
  
  // Initialize the BLE keyboard
  bleKeyboard.begin();
  Serial.println("BLE Slide Presenter initialized. Waiting for connection...");
  Serial.println("Controls:");
  Serial.println("- Main button: single=ESC/F5 toggle, double=blink, hold=LED ON");
  Serial.println("- Button 5: Page Up");
  Serial.println("- Button 6: Page Down");
  Serial.println("Current mode: ESC");
}

void loop() {
  // Update all buttons
  mainBtn.update();
  pageUpBtn.update();
  pageDownBtn.update();
  
  unsigned long now = millis();
  
  // Handle Page Up button
  if (pageUpBtn.fell()) {
    if (bleKeyboard.isConnected()) {
      Serial.println("Page Up pressed");
      bleKeyboard.write(KEY_PAGE_UP);
    } else {
      Serial.println("Page Up pressed (not connected)");
    }
  }
  
  // Handle Page Down button
  if (pageDownBtn.fell()) {
    if (bleKeyboard.isConnected()) {
      Serial.println("Page Down pressed");
      bleKeyboard.write(KEY_PAGE_DOWN);
    } else {
      Serial.println("Page Down pressed (not connected)");
    }
  }
  
  // Handle main button pressed
  if (mainBtn.fell()) {
    lastTapTime = now;
    holding = true;
    
    if (waitingForSecondTap) {
      // Second tap detected
      tapCount = 2;
      waitingForSecondTap = false;
    } else {
      // First tap
      tapCount = 1;
      waitingForSecondTap = true;
    }
  }
  
  // Handle main button released
  if (mainBtn.rose()) {
    unsigned long heldTime = now - lastTapTime;
    holding = false;  // Clear holding flag immediately
    
    if (heldTime > holdThreshold) {
      // HOLD RELEASED
      Serial.println("Hold released -> LED OFF");
      stopBlinking();
      digitalWrite(LED_PIN, LOW);
      tapCount = 0;
      waitingForSecondTap = false;
    } else {
      // TAP DETECTED
      if (tapCount == 2) {
        // Double tap immediately processed
        Serial.println("Double tap -> blink toggle");
        if (blinkingActive) {
          stopBlinking();
        } else {
          startBlinking();
        }
        tapCount = 0;
        waitingForSecondTap = false;
      }
      // Single tap will be processed after timeout
    }
  }
  
  // Check for single tap timeout (only if not currently holding)
  if (waitingForSecondTap && !holding && (now - lastTapTime > doubleTapThreshold)) {
    if (tapCount == 1) {
      // Single tap - toggle between ESC and F5
      if (bleKeyboard.isConnected()) {
        if (escMode) {
          Serial.println("Single tap -> ESC");
          bleKeyboard.write(KEY_ESC);
        } else {
          Serial.println("Single tap -> F5");
          bleKeyboard.write(KEY_F5);
        }
        escMode = !escMode;  // Toggle mode
        Serial.print("Mode switched to: ");
        Serial.println(escMode ? "ESC" : "F5");
      } else {
        Serial.println("Single tap (not connected)");
      }
    }
    tapCount = 0;
    waitingForSecondTap = false;
  }
  
  // While holding main button
  if (holding && mainBtn.read() == LOW) {
    unsigned long heldTime = now - lastTapTime;
    if (heldTime > holdThreshold) {
      Serial.println("Holding -> LED ON");
      stopBlinking();  // Stop blinking when hold is detected
      digitalWrite(LED_PIN, HIGH);
    }
  }
  
  // Show connection status periodically
  static unsigned long lastStatusTime = 0;
  if (now - lastStatusTime > 5000) {  // Every 5 seconds
    lastStatusTime = now;
    if (bleKeyboard.isConnected()) {
      Serial.println("BLE Connected");
    } else {
      Serial.println("BLE Disconnected - waiting for connection...");
    }
  }
  
  // Small delay to prevent excessive CPU usage
  vTaskDelay(pdMS_TO_TICKS(5));
}

void startBlinking() {
  blinkingActive = true;
  Serial.println("Blinking started");
}

void stopBlinking() {
  blinkingActive = false;
  digitalWrite(LED_PIN, LOW);  // Ensure LED is off when stopping
  Serial.println("Blinking stopped");
}

void blinkTask(void *parameter) {
  while (true) {
    if (blinkingActive) {
      // Blink ON
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(500));  // ON for 500ms
      
      // Check if still should be blinking
      if (!blinkingActive) {
        digitalWrite(LED_PIN, LOW);
        continue;
      }
      
      // Blink OFF
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(500));  // OFF for 500ms
    } else {
      // Not blinking - ensure LED is OFF and wait
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms when not blinking
    }
  }
}